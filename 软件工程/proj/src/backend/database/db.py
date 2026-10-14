from __future__ import annotations

import sqlite3
from pathlib import Path

from backend.utils.password import hash_password

DB_PATH = Path(__file__).resolve().parent / "canteen.sqlite3"

TARGET_CANTEENS = ["康桥苑", "梧桐苑"]
CATEGORY_OPTIONS = [
    "面条",
    "饺子馄饨",
    "米饭套餐",
    "盖饭",
    "炒饭",
    "拌饭",
    "米线粉丝",
    "快餐",
    "轻食",
    "汤食",
    "小吃",
    "夜宵烧烤",
]
MIN_STALL_COUNT = 28
MIN_TAG_COUNT = 16


def get_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def _column_names(conn: sqlite3.Connection, table: str):
    rows = conn.execute(f"PRAGMA table_info({table})").fetchall()
    return {row["name"] for row in rows}


def _migrate_user_table(conn: sqlite3.Connection):
    columns = _column_names(conn, "user")
    if "signature" not in columns:
        conn.execute("ALTER TABLE user ADD COLUMN signature TEXT DEFAULT NULL")
    if "preference_text" not in columns:
        conn.execute("ALTER TABLE user ADD COLUMN preference_text TEXT DEFAULT NULL")
    pass  # role=2 已废弃，无需迁移


def init_db():
    conn = get_connection()
    cur = conn.cursor()
    cur.executescript(
        """
        CREATE TABLE IF NOT EXISTS user (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT NOT NULL UNIQUE,
            username TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            role INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL DEFAULT 1,
            avatar_url TEXT DEFAULT NULL,
            signature TEXT DEFAULT NULL,
            preference_text TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS canteen (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            location TEXT DEFAULT NULL,
            description TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS stall (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            canteen_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            category TEXT DEFAULT NULL,
            description TEXT DEFAULT NULL,
            avg_rating REAL NOT NULL DEFAULT 0,
            review_count INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(canteen_id, name),
            FOREIGN KEY (canteen_id) REFERENCES canteen(id)
        );

        CREATE TABLE IF NOT EXISTS tag (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS stall_tag (
            stall_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            PRIMARY KEY (stall_id, tag_id),
            FOREIGN KEY (stall_id) REFERENCES stall(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tag(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS review (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            rating INTEGER NOT NULL,
            content TEXT DEFAULT NULL,
            is_deleted INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );

        CREATE TABLE IF NOT EXISTS favorite (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );

        CREATE TABLE IF NOT EXISTS blacklist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );

        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            visited_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );

        CREATE INDEX IF NOT EXISTS idx_stall_category ON stall(category);
        CREATE INDEX IF NOT EXISTS idx_stall_avg_rating ON stall(avg_rating);
        CREATE INDEX IF NOT EXISTS idx_stall_review_count ON stall(review_count);
        CREATE INDEX IF NOT EXISTS idx_review_stall_id ON review(stall_id);
        CREATE INDEX IF NOT EXISTS idx_review_user_id ON review(user_id);
        CREATE INDEX IF NOT EXISTS idx_review_created_at ON review(created_at);
        """
    )
    _migrate_user_table(conn)
    conn.commit()
    seed_users(conn)
    ensure_core_dataset(conn)
    conn.close()


def seed_users(conn: sqlite3.Connection):
    default_users = [
        {
            "student_id": "admin001",
            "username": "管理员",
            "password": "admin123",
            "role": 1,
            "status": 1,
            "avatar_url": None,
            "signature": "负责系统权限和账号管理",
            "preference_text": "喜欢评分高、出餐快、口味稳定的窗口",
        },
        {
            "student_id": "2230123456",
            "username": "张三",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "今天想吃点热乎的",
            "preference_text": "喜欢面条、微辣、性价比高的窗口",
        },
        {
            "student_id": "2230123457",
            "username": "李四",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "偏爱米饭和盖饭",
            "preference_text": "喜欢米饭套餐、分量足、出餐快的窗口",
        },
        {
            "student_id": "2230123458",
            "username": "王五",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "减脂期，尽量吃清淡",
            "preference_text": "清淡、低油、轻食、适合减脂",
        },
        {
            "student_id": "2230123459",
            "username": "赵六",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "无辣不欢",
            "preference_text": "重口味、辣、香锅、烧烤",
        },
        {
            "student_id": "2230123460",
            "username": "陈七",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "面食爱好者",
            "preference_text": "面条、饺子、馄饨、汤类",
        },
        {
            "student_id": "2230123461",
            "username": "刘八",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "夜猫子，经常吃夜宵",
            "preference_text": "夜宵、烧烤、小吃、分量足",
        },
        {
            "student_id": "2230123462",
            "username": "孙九",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "预算有限的大一新生",
            "preference_text": "性价比高、分量足、快餐",
        },
        {
            "student_id": "2230123463",
            "username": "周十",
            "password": "123456",
            "role": 0,
            "status": 1,
            "avatar_url": None,
            "signature": "胃不太好，偏爱软烂好消化的",
            "preference_text": "清淡、汤食、粥、软烂好消化",
        },
    ]

    for item in default_users:
        exists = conn.execute("SELECT id FROM user WHERE student_id = ?", (item["student_id"],)).fetchone()
        if exists:
            continue
        conn.execute(
            """
            INSERT INTO user (
                student_id, username, password_hash, role, status, avatar_url, signature, preference_text
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                item["student_id"],
                item["username"],
                hash_password(item["password"]),
                item["role"],
                item["status"],
                item["avatar_url"],
                item["signature"],
                item["preference_text"],
            ),
        )
    conn.commit()


def _should_reseed(conn: sqlite3.Connection):
    current_canteens = [row["name"] for row in conn.execute("SELECT name FROM canteen ORDER BY id").fetchall()]
    stall_rows = conn.execute("SELECT name, category FROM stall ORDER BY id").fetchall()
    tag_count = conn.execute("SELECT COUNT(*) FROM tag").fetchone()[0]
    if current_canteens != TARGET_CANTEENS:
        return True
    if len(stall_rows) < MIN_STALL_COUNT or tag_count < MIN_TAG_COUNT:
        return True
    if any("?" in row["name"] for row in stall_rows):
        return True
    if any((row["category"] or "") not in CATEGORY_OPTIONS for row in stall_rows):
        return True
    return False


def ensure_core_dataset(conn: sqlite3.Connection):
    if not _should_reseed(conn):
        return

    conn.execute("DELETE FROM history")
    conn.execute("DELETE FROM favorite")
    conn.execute("DELETE FROM blacklist")
    conn.execute("DELETE FROM review")
    conn.execute("DELETE FROM stall_tag")
    conn.execute("DELETE FROM stall")
    conn.execute("DELETE FROM tag")
    conn.execute("DELETE FROM canteen")
    conn.execute(
        "DELETE FROM sqlite_sequence WHERE name IN ('canteen', 'stall', 'tag', 'review', 'favorite', 'blacklist', 'history')"
    )
    conn.commit()

    canteens = [
        ("康桥苑", "兴庆校区西区", "米饭套餐、快餐和轻食选择较多，适合赶课时快速吃饭。"),
        ("梧桐苑", "兴庆校区东区", "面条、汤食、夜宵和小吃更丰富，整体选择更杂。"),
    ]
    conn.executemany("INSERT INTO canteen (name, location, description) VALUES (?, ?, ?)", canteens)
    canteen_map = {row["name"]: row["id"] for row in conn.execute("SELECT id, name FROM canteen").fetchall()}

    tags = [
        ("微辣", "适合能吃一点辣的同学"),
        ("重口味", "整体调味偏重，更下饭"),
        ("分量足", "主打量大，适合饿的时候"),
        ("出餐快", "高峰期也比较快"),
        ("性价比高", "价格实惠，适合常吃"),
        ("清淡", "适合不想吃太油太辣"),
        ("汤类", "带汤、暖胃"),
        ("夜宵", "适合晚归和加餐"),
        ("小吃", "偏零食和轻加餐"),
        ("素菜多", "配菜和蔬菜选项更多"),
        ("肉量足", "肉菜更扎实"),
        ("适合减脂", "轻食、低负担"),
        ("面条", "适合偏爱面食口感的同学"),
        ("现做现卖", "新鲜度高，现点现做"),
        ("排队多", "人气旺，高峰期需要等"),
        ("陕西风味", "融合本地口味特色"),
    ]
    conn.executemany("INSERT INTO tag (name, description) VALUES (?, ?)", tags)
    tag_map = {row["name"]: row["id"] for row in conn.execute("SELECT id, name FROM tag").fetchall()}

    stalls = [
        # 康桥苑 (10个窗口)
        ("康桥苑", "黄焖鸡米饭窗口", "米饭套餐", "黄焖鸡配米饭稳定，出餐快，适合赶课。"),
        ("康桥苑", "麻辣香锅窗口", "快餐", "可自选荤素和辣度，整体偏重口。"),
        ("康桥苑", "兰州拉面窗口", "面条", "牛肉拉面和刀削面都比较稳，汤热面筋道。"),
        ("康桥苑", "铁板炒饭窗口", "炒饭", "炒饭炒面香气足，分量很足。"),
        ("康桥苑", "砂锅米线窗口", "米线粉丝", "汤底热，米线口感稳定，适合冷天。"),
        ("康桥苑", "轻食沙拉窗口", "轻食", "鸡胸肉、玉米和水果搭配，整体清淡。"),
        ("康桥苑", "猪脚饭窗口", "盖饭", "猪脚入味，酱汁浓，适合想吃肉的时候。"),
        ("康桥苑", "鸡排小吃窗口", "小吃", "鸡排、薯条和烤肠类小吃，适合加餐。"),
        ("康桥苑", "酸辣粉窗口", "米线粉丝", "酸辣味明显，粉条吸汤，晚上也常有人买。"),
        ("康桥苑", "肉夹馍窗口", "小吃", "陕西本地风味，馍酥肉香，现做现卖。"),
        ("康桥苑", "蒸饭窗口", "米饭套餐", "蒸饭配多种小菜，清淡健康，适合不想吃太油的时候。"),
        ("康桥苑", "麻辣烫窗口", "快餐", "自选食材，汤底浓郁，冬天特别受欢迎。"),
        # 梧桐苑 (16个窗口)
        ("梧桐苑", "牛肉面窗口", "面条", "现煮牛肉面稳定，属于不容易踩雷的窗口。"),
        ("梧桐苑", "自选盖饭窗口", "盖饭", "可按自己口味搭配荤素，适合天天吃。"),
        ("梧桐苑", "重庆小面窗口", "面条", "辣味更冲，适合重口味和嗜辣人群。"),
        ("梧桐苑", "香锅拌饭窗口", "拌饭", "酱香重，拌饭比较香，吃起来满足感强。"),
        ("梧桐苑", "馄饨水饺窗口", "饺子馄饨", "馄饨和水饺都有汤，暖胃。"),
        ("梧桐苑", "夜宵烧烤窗口", "夜宵烧烤", "晚上营业更久，适合夜里加餐。"),
        ("梧桐苑", "砂锅粥窗口", "汤食", "粥和砂锅类比较清淡，晚饭很合适。"),
        ("梧桐苑", "炸酱面窗口", "面条", "拌面酱香足，配菜也比较完整。"),
        ("梧桐苑", "小笼包蒸饺窗口", "饺子馄饨", "早点和晚饭都能吃，蒸点类出餐稳。"),
        ("梧桐苑", "土豆粉窗口", "米线粉丝", "土豆粉劲道，汤底偏辣，性价比不错。"),
        ("梧桐苑", "烤鸭饭窗口", "盖饭", "烤鸭片配米饭，酱汁入味，分量实在。"),
        ("梧桐苑", "凉皮凉面窗口", "小吃", "陕西特色凉皮，夏天必吃，酸辣爽口。"),
        ("梧桐苑", "番茄鸡蛋面窗口", "面条", "家常口味，汤清味鲜，适合不想吃重口的时候。"),
        ("梧桐苑", "卤肉饭窗口", "米饭套餐", "台式卤肉饭风格，卤汁浓郁，肥而不腻。"),
        ("梧桐苑", "豆浆油条窗口", "小吃", "早餐首选，豆浆现磨，油条酥脆。"),
        ("梧桐苑", "蛋炒饭窗口", "炒饭", "简单扎实，鸡蛋炒饭粒粒分明，加腊肠更香。"),
    ]
    conn.executemany(
        """
        INSERT INTO stall (canteen_id, name, category, description, avg_rating, review_count, status)
        VALUES (?, ?, ?, ?, 0, 0, 1)
        """,
        [
            (canteen_map[canteen_name], name, category, description)
            for canteen_name, name, category, description in stalls
        ],
    )
    stall_map = {row["name"]: row["id"] for row in conn.execute("SELECT id, name FROM stall").fetchall()}

    stall_tags = {
        "黄焖鸡米饭窗口": ["分量足", "出餐快", "性价比高", "肉量足"],
        "麻辣香锅窗口": ["微辣", "重口味", "素菜多", "肉量足", "排队多"],
        "兰州拉面窗口": ["汤类", "性价比高", "出餐快", "面条"],
        "铁板炒饭窗口": ["分量足", "重口味", "出餐快"],
        "砂锅米线窗口": ["汤类", "微辣", "出餐快"],
        "轻食沙拉窗口": ["清淡", "适合减脂", "素菜多", "出餐快"],
        "猪脚饭窗口": ["分量足", "重口味", "肉量足"],
        "鸡排小吃窗口": ["小吃", "出餐快", "重口味"],
        "酸辣粉窗口": ["微辣", "汤类", "小吃", "性价比高"],
        "肉夹馍窗口": ["小吃", "陕西风味", "现做现卖", "性价比高"],
        "蒸饭窗口": ["清淡", "素菜多", "适合减脂", "出餐快"],
        "麻辣烫窗口": ["微辣", "重口味", "汤类", "排队多", "素菜多"],
        "牛肉面窗口": ["汤类", "性价比高", "出餐快", "面条"],
        "自选盖饭窗口": ["分量足", "出餐快", "性价比高", "素菜多"],
        "重庆小面窗口": ["微辣", "重口味", "出餐快", "面条"],
        "香锅拌饭窗口": ["重口味", "分量足", "肉量足"],
        "馄饨水饺窗口": ["清淡", "汤类", "出餐快"],
        "夜宵烧烤窗口": ["夜宵", "小吃", "重口味"],
        "砂锅粥窗口": ["清淡", "汤类", "适合减脂"],
        "炸酱面窗口": ["重口味", "性价比高", "面条"],
        "小笼包蒸饺窗口": ["清淡", "小吃", "出餐快", "现做现卖"],
        "土豆粉窗口": ["微辣", "汤类", "性价比高", "面条"],
        "烤鸭饭窗口": ["分量足", "肉量足", "出餐快"],
        "凉皮凉面窗口": ["微辣", "陕西风味", "小吃", "性价比高"],
        "番茄鸡蛋面窗口": ["清淡", "汤类", "面条", "出餐快"],
        "卤肉饭窗口": ["肉量足", "重口味", "分量足"],
        "豆浆油条窗口": ["小吃", "清淡", "性价比高", "出餐快"],
        "蛋炒饭窗口": ["出餐快", "性价比高", "分量足"],
    }
    conn.executemany(
        "INSERT INTO stall_tag (stall_id, tag_id) VALUES (?, ?)",
        [
            (stall_map[stall_name], tag_map[tag_name])
            for stall_name, tag_names in stall_tags.items()
            for tag_name in tag_names
        ],
    )

    user_map = {row["student_id"]: row["id"] for row in conn.execute("SELECT id, student_id FROM user").fetchall()}
    reviews = [
        # 黄焖鸡米饭窗口
        ("2230123456", "黄焖鸡米饭窗口", 4, "赶八点课的救星，拿了就走，鸡肉不柴，米饭给得实在。", 0),
        ("2230123457", "黄焖鸡米饭窗口", 5, "吃了一学期了，每次都不踩雷，汤汁拌饭超香。", 0),
        ("admin001", "黄焖鸡米饭窗口", 4, "稳定是最大的优点，不会有惊喜但也不会失望。", 0),
        ("2230123458", "黄焖鸡米饭窗口", 4, "减脂期偶尔破戒就选这个，罪恶感没那么重哈哈。", 0),
        ("2230123462", "黄焖鸡米饭窗口", 5, "大一刚来不知道吃什么，学长推荐的，果然没错，性价比超高。", 0),
        # 麻辣香锅窗口
        ("2230123456", "麻辣香锅窗口", 4, "自选的感觉很好，想吃什么夹什么，辣度可以调，中辣刚好。", 0),
        ("admin001", "麻辣香锅窗口", 3, "中午高峰期排队要二十分钟，赶时间慎选，但味道确实不错。", 0),
        ("2230123459", "麻辣香锅窗口", 5, "终于找到一个辣度够的窗口！重辣选项真的很过瘾，下饭神器。", 0),
        ("2230123461", "麻辣香锅窗口", 4, "晚上来吃人少很多，慢慢选食材很享受，比中午体验好多了。", 0),
        # 兰州拉面窗口
        ("2230123456", "兰州拉面窗口", 5, "汤是真的鲜，牛肉片给得不少，面条劲道，冬天必吃。", 0),
        ("2230123457", "兰州拉面窗口", 4, "刀削面比拉面更推，口感更有嚼劲，价格也合理。", 0),
        ("2230123460", "兰州拉面窗口", 5, "面食爱好者的天堂，汤底清亮不油腻，每周至少来两次。", 0),
        ("2230123463", "兰州拉面窗口", 4, "胃不好的时候喝汤特别舒服，面条软一点要就行，师傅很配合。", 0),
        # 铁板炒饭窗口
        ("2230123457", "铁板炒饭窗口", 4, "铁板出来的香气真的很勾人，蛋炒饭粒粒分明，分量给足了。", 0),
        ("admin001", "铁板炒饭窗口", 3, "油稍微大了点，吃完有点腻，但偶尔解馋还是不错的。", 0),
        ("2230123459", "铁板炒饭窗口", 5, "加辣椒酱的铁板炒饭绝了，香辣下饭，吃完整个人都精神了。", 0),
        # 砂锅米线窗口
        ("2230123457", "砂锅米线窗口", 5, "汤底真的很暖，冬天下课冻得半死来一碗，整个人都活了。", 0),
        ("admin001", "砂锅米线窗口", 4, "配料挺丰富的，豆皮和肉丸都有，性价比不错。", 0),
        ("2230123456", "砂锅米线窗口", 5, "微辣口味刚好，不会太辣但有层次感，出餐速度也快。", 0),
        ("2230123463", "砂锅米线窗口", 5, "胃不舒服的时候来一碗热汤米线，比吃药管用，暖到心里去了。", 0),
        # 轻食沙拉窗口
        ("2230123456", "轻食沙拉窗口", 4, "减脂期的好朋友，鸡胸肉嫩，蔬菜新鲜，酱汁选低卡的很满足。", 0),
        ("2230123458", "轻食沙拉窗口", 5, "终于有个不用担心热量的窗口！每天中午必来，皮肤感觉都变好了。", 0),
        ("2230123460", "轻食沙拉窗口", 3, "分量对我来说有点少，下午三点就饿了，可能适合女生多一些。", 0),
        # 猪脚饭窗口
        ("2230123457", "猪脚饭窗口", 4, "猪脚炖得很烂，酱汁拌饭超香，吃完有种满足感。", 0),
        ("admin001", "猪脚饭窗口", 5, "这个窗口被低估了，猪脚入味程度比外面很多店都强。", 0),
        ("2230123461", "猪脚饭窗口", 4, "晚上来吃猪脚饭，配个汤，一天的疲惫都消了。", 0),
        # 鸡排小吃窗口
        ("2230123456", "鸡排小吃窗口", 4, "下午四点饿了来一块鸡排，刚炸出来的脆皮真的香，撒点椒盐完美。", 0),
        ("2230123461", "鸡排小吃窗口", 5, "夜宵前的开胃小食，鸡排加薯条，边走边吃很方便。", 0),
        ("2230123462", "鸡排小吃窗口", 4, "价格实惠，鸡排分量给得足，比外卖便宜多了。", 0),
        # 酸辣粉窗口
        ("2230123457", "酸辣粉窗口", 4, "酸辣味很正，粉条吸饱了汤汁，花生碎和葱花点睛之笔。", 0),
        ("2230123459", "酸辣粉窗口", 5, "辣度够劲，酸味也到位，吃完额头微微出汗，爽！", 0),
        ("2230123462", "酸辣粉窗口", 4, "性价比很高，一碗下来很饱，晚上当主食完全够。", 0),
        # 肉夹馍窗口
        ("2230123456", "肉夹馍窗口", 5, "陕西人表示这个肉夹馍做得很正宗，馍酥肉香，汁水足。", 0),
        ("2230123459", "肉夹馍窗口", 4, "现做现卖，馍是热的，肉是软的，配辣椒更好吃。", 0),
        ("2230123462", "肉夹馍窗口", 5, "五块钱一个，吃一个就饱了，性价比天花板，每周必吃。", 0),
        ("2230123460", "肉夹馍窗口", 4, "外地来的同学一定要试试，这才是真正的陕西味道。", 0),
        # 蒸饭窗口
        ("2230123458", "蒸饭窗口", 5, "减脂神器！蒸饭配蒸蔬菜，清淡健康，吃完不会有负担感。", 0),
        ("2230123463", "蒸饭窗口", 4, "胃不好的时候来这里，软烂好消化，小菜种类也挺多的。", 0),
        ("2230123457", "蒸饭窗口", 3, "口味偏淡，重口味的同学可能会觉得没味道，但健康是真的。", 0),
        # 麻辣烫窗口
        ("2230123459", "麻辣烫窗口", 5, "自选食材太爽了，什么都能涮，汤底浓郁，冬天排队也值得。", 0),
        ("2230123456", "麻辣烫窗口", 4, "中午人很多，但翻台快，等个十分钟就能吃上，味道不错。", 0),
        ("2230123461", "麻辣烫窗口", 4, "晚上来人少，可以慢慢选，宽粉和豆腐皮是必点的。", 0),
        ("2230123462", "麻辣烫窗口", 3, "价格按重量算，不小心选多了有点贵，要控制一下量。", 0),
        # 牛肉面窗口
        ("2230123456", "牛肉面窗口", 5, "梧桐苑最稳的窗口没有之一，汤清亮，牛肉片厚，面条有嚼劲。", 0),
        ("2230123457", "牛肉面窗口", 4, "高峰期确实要排队，但等了也不亏，味道很稳定。", 0),
        ("admin001", "牛肉面窗口", 5, "推荐给每个新来的同学，基本不会踩雷，是食堂的门面担当。", 0),
        ("2230123460", "牛肉面窗口", 5, "面食爱好者的首选，汤底鲜美，加辣椒油更香，每次都吃光。", 0),
        ("2230123463", "牛肉面窗口", 4, "汤很暖胃，牛肉炖得软烂，胃不好的时候来这里很合适。", 0),
        # 自选盖饭窗口
        ("2230123457", "自选盖饭窗口", 4, "搭配自由度很高，今天想吃什么菜自己选，天天吃不腻。", 0),
        ("admin001", "自选盖饭窗口", 4, "菜品更新比较稳定，不会一直是那几样，值得常来。", 0),
        ("2230123458", "自选盖饭窗口", 4, "可以选蒸蔬菜和豆腐，减脂期也能吃，很友好。", 0),
        ("2230123462", "自选盖饭窗口", 5, "分量给得足，价格实惠，大一穷学生的最爱，每天中午必来。", 0),
        # 重庆小面窗口
        ("2230123456", "重庆小面窗口", 5, "辣度真的够，麻辣鲜香全都有，吃完嘴巴红红的很过瘾。", 0),
        ("admin001", "重庆小面窗口", 4, "喜欢辣的同学基本都会满意，不喜欢辣的就别来了哈哈。", 0),
        ("2230123459", "重庆小面窗口", 5, "终于找到辣度够的面条！加了辣椒油和花椒，麻辣双全，爱了。", 0),
        ("2230123460", "重庆小面窗口", 4, "面条很有嚼劲，酱料调得很香，就是对不能吃辣的朋友不太友好。", 0),
        # 香锅拌饭窗口
        ("2230123457", "香锅拌饭窗口", 4, "酱香味很浓，拌进米饭里特别香，肉菜比例也合理。", 0),
        ("admin001", "香锅拌饭窗口", 4, "重口味里比较有特色的一个，偶尔换换口味很不错。", 0),
        ("2230123459", "香锅拌饭窗口", 5, "香锅的香气老远就能闻到，每次路过都忍不住进来，上瘾了。", 0),
        ("2230123461", "香锅拌饭窗口", 4, "晚上吃这个很满足，分量足，吃完撑撑的去图书馆。", 0),
        # 馄饨水饺窗口
        ("2230123456", "馄饨水饺窗口", 5, "馄饨皮薄馅大，汤底清鲜，晚上吃完整个人都暖了。", 0),
        ("2230123457", "馄饨水饺窗口", 4, "水饺煮得很到位，不破皮，蘸醋吃很清爽。", 0),
        ("2230123460", "馄饨水饺窗口", 5, "面食爱好者必来！馄饨汤加点辣椒油，完美，每周至少来一次。", 0),
        ("2230123463", "馄饨水饺窗口", 5, "胃不好的时候来吃馄饨，汤清淡，馅料软，吃完很舒服。", 0),
        # 夜宵烧烤窗口
        ("2230123456", "夜宵烧烤窗口", 4, "晚上十点还开着，烤串香气飘出来老远，熬夜必备。", 0),
        ("2230123461", "夜宵烧烤窗口", 5, "夜猫子的福音！烤鸡翅和烤玉米是必点，配个饮料完美收尾。", 0),
        ("2230123459", "夜宵烧烤窗口", 4, "烤串撒孜然和辣椒粉，香气扑鼻，就是有时候等的时间稍长。", 0),
        # 砂锅粥窗口
        ("2230123457", "砂锅粥窗口", 4, "皮蛋瘦肉粥熬得很浓稠，晚上喝一碗，肠胃很舒服。", 0),
        ("admin001", "砂锅粥窗口", 4, "清淡养胃，适合不想吃太重的时候，配个小菜刚好。", 0),
        ("2230123463", "砂锅粥窗口", 5, "胃炎发作的时候来这里，白粥加小菜，吃完感觉好多了，救命窗口。", 0),
        ("2230123458", "砂锅粥窗口", 4, "减脂期的好选择，粥热量低，配蒸蔬菜，吃完不会有负担。", 0),
        # 炸酱面窗口
        ("2230123456", "炸酱面窗口", 4, "炸酱拌得很香，黄瓜丝和豆芽配料齐全，吃起来很有层次感。", 0),
        ("2230123460", "炸酱面窗口", 5, "北方面食的感觉，炸酱咸香，面条劲道，拌匀了每一口都很满足。", 0),
        ("2230123462", "炸酱面窗口", 4, "价格实惠，分量够，炸酱味道正，性价比很高的一个窗口。", 0),
        # 小笼包蒸饺窗口
        ("2230123457", "小笼包蒸饺窗口", 4, "蒸饺皮薄馅足，咬开有汤汁，早上来一笼很满足。", 0),
        ("2230123460", "小笼包蒸饺窗口", 5, "小笼包汤汁丰富，皮不厚，蘸姜醋吃，早餐的最高境界。", 0),
        ("2230123463", "小笼包蒸饺窗口", 4, "蒸点类比较好消化，胃不好的时候早餐来这里，很温和。", 0),
        # 土豆粉窗口
        ("2230123456", "土豆粉窗口", 4, "土豆粉比米线更劲道，汤底偏辣，加了醋更开胃，很喜欢。", 0),
        ("2230123459", "土豆粉窗口", 5, "辣度刚好，土豆粉吸汤能力超强，每一口都是精华，强烈推荐。", 0),
        ("2230123462", "土豆粉窗口", 4, "性价比不错，分量给得足，比外面的土豆粉店便宜一半。", 0),
        # 烤鸭饭窗口
        ("2230123457", "烤鸭饭窗口", 4, "烤鸭片皮脆肉嫩，酱汁拌饭很香，分量给得实在。", 0),
        ("admin001", "烤鸭饭窗口", 4, "食堂里比较少见的烤鸭饭，做得还不错，值得一试。", 0),
        ("2230123461", "烤鸭饭窗口", 5, "晚上来吃烤鸭饭，配个汤，感觉比外面餐厅还好吃，性价比超高。", 0),
        # 凉皮凉面窗口
        ("2230123456", "凉皮凉面窗口", 5, "陕西凉皮做得很正宗，面筋Q弹，辣椒油香，夏天必吃，神仙窗口。", 0),
        ("2230123459", "凉皮凉面窗口", 5, "辣椒油调得很香，凉皮爽滑，吃完整个人都清爽了，夏天救星。", 0),
        ("2230123458", "凉皮凉面窗口", 4, "热量不高，夏天减脂期吃这个很合适，就是辣椒要少放。", 0),
        ("2230123462", "凉皮凉面窗口", 4, "外地同学来了一定要试试陕西凉皮，跟家乡的凉皮完全不一样。", 0),
        # 番茄鸡蛋面窗口
        ("2230123456", "番茄鸡蛋面窗口", 4, "家常味道，汤清鲜，番茄酸甜，鸡蛋嫩，吃完很舒服。", 0),
        ("2230123463", "番茄鸡蛋面窗口", 5, "胃不好的时候来这里，汤温和不刺激，面条软烂，像妈妈做的。", 0),
        ("2230123458", "番茄鸡蛋面窗口", 4, "清淡低负担，减脂期也能吃，番茄的酸味很开胃。", 0),
        # 卤肉饭窗口
        ("2230123457", "卤肉饭窗口", 5, "卤汁浓郁，肥肉部分入口即化，拌进米饭里每一口都是享受。", 0),
        ("2230123459", "卤肉饭窗口", 4, "重口味爱好者必来，卤汁咸香，配个卤蛋更完美。", 0),
        ("2230123461", "卤肉饭窗口", 5, "晚上吃卤肉饭，满满的幸福感，分量足，吃完撑撑的去自习。", 0),
        # 豆浆油条窗口
        ("2230123456", "豆浆油条窗口", 5, "早上七点来，豆浆还是热的，油条刚炸出来，酥脆无比，完美早餐。", 0),
        ("2230123460", "豆浆油条窗口", 4, "豆浆浓度刚好，不会太稀，油条蘸豆浆吃，经典搭配。", 0),
        ("2230123462", "豆浆油条窗口", 5, "最便宜的早餐，两块钱一根油条，豆浆免费续，穷学生的福音。", 0),
        # 蛋炒饭窗口
        ("2230123457", "蛋炒饭窗口", 4, "炒饭粒粒分明，鸡蛋炒得嫩，加腊肠版本更香，简单扎实。", 0),
        ("2230123462", "蛋炒饭窗口", 5, "性价比超高，一份炒饭吃得很饱，出餐快，赶时间的首选。", 0),
        ("2230123459", "蛋炒饭窗口", 3, "口味偏淡，重口味的我加了很多酱油才够味，建议多放点盐。", 0),
    ]
    conn.executemany(
        "INSERT INTO review (user_id, stall_id, rating, content, is_deleted) VALUES (?, ?, ?, ?, ?)",
        [
            (user_map[student_id], stall_map[stall_name], rating, content, is_deleted)
            for student_id, stall_name, rating, content, is_deleted in reviews
        ],
    )

    conn.execute(
        """
        UPDATE stall
        SET review_count = (
                SELECT COUNT(*)
                FROM review r
                WHERE r.stall_id = stall.id AND r.is_deleted = 0
            ),
            avg_rating = COALESCE((
                SELECT ROUND(AVG(r.rating), 2)
                FROM review r
                WHERE r.stall_id = stall.id AND r.is_deleted = 0
            ), 0),
            updated_at = CURRENT_TIMESTAMP
        """
    )
    conn.commit()
