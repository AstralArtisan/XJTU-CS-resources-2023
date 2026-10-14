# 西交食堂评价系统

前后端分离的食堂评价系统，面向西安交通大学两个食堂：康桥苑、梧桐苑。

## 快速启动

在项目根目录执行：

```bash
python start.py
```

自动启动后端（8000端口）+ 前端（5173端口）并打开浏览器。

### 手动启动

后端（在项目根目录）：

```bash
cd src
python -m backend.main
```

前端（另开终端）：

```bash
cd src/frontend
python -m http.server 5173
```

## 项目结构

```
proj/
├── start.py                        # 一键启动脚本
├── src/
│   ├── frontend/
│   │   ├── index.html
│   │   └── src/
│   │       ├── main.js             # 所有页面逻辑（SPA hash路由）
│   │       ├── api/client.js       # Fetch 封装
│   │       ├── store/user.js       # localStorage 状态管理
│   │       └── styles/app.css      # 校园活力设计系统
│   └── backend/
│       ├── main.py                 # HTTP 路由（ThreadingHTTPServer）
│       ├── database/
│       │   ├── db.py               # 建表 + 种子数据
│       │   └── connection.py       # SQLite 连接
│       ├── services/               # 业务逻辑层
│       └── utils/                  # JWT、密码哈希、响应工具
└── docs/
    └── reports/
        └── 文档审查报告.md
```

## 功能

### 普通用户

- 注册、登录、退出
- 修改个人资料（昵称、签名、口味偏好、头像）
- 修改密码
- 浏览窗口列表，按食堂/分类/标签/关键词筛选
- 查看窗口详情与评论
- 提交、修改、删除自己的评论
- 收藏窗口 / 取消收藏
- 拉黑窗口 / 移除黑名单
- 浏览历史自动记录（访问窗口详情时自动触发）
- 查看排行榜（评分榜、热度榜、最新评价榜）
- **今天吃什么**：首页随机推荐弹窗，可反复刷新
- **AI 美食助手**：右下角浮动按钮，侧边聊天面板，输入需求获取推荐

### 管理员

- 标签管理（新增、修改）
- 食堂信息管理
- 窗口管理（新增、修改、停用）
- 评论删除
- 用户管理（设为管理员 / 恢复普通用户）

## 角色体系

| role | 说明 |
|------|------|
| 0 | 普通用户（注册默认） |
| 1 | 管理员（拥有全部后台权限） |

## 默认账号

| 账号 | 密码 | 角色 |
|------|------|------|
| admin001 | admin123 | 管理员 |
| 2230123456 | 123456 | 普通用户 |

## 推荐系统

### 今天吃什么（随机推荐）

首页金色按钮，每次随机推荐一个窗口，排除黑名单，可反复刷新。

### AI 美食助手

右下角 🤖 按钮打开聊天面板，输入需求（如"想吃辣的"），后端调用 OpenRouter / Qwen 返回推荐结果和中文总结。

OpenRouter API Key 已内置，无需手动配置。如需替换，设置环境变量即可覆盖：

```bash
set OPENROUTER_API_KEY=你的key
```

如果 API 调用失败，系统自动降级为本地算法推荐。

## 数据库

SQLite，共 9 张表：`user`、`canteen`、`stall`、`tag`、`stall_tag`、`review`、`favorite`、`blacklist`、`history`。

启动时自动初始化种子数据（2个食堂、18个窗口、多组标签和评论）。

## API 端点

认证：
- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/auth/me`
- `POST /api/auth/logout`
- `PUT /api/auth/password`

浏览：
- `GET /api/canteens`
- `GET /api/categories`
- `GET /api/tags`
- `GET /api/stalls` （支持 canteen_id / category / tag_name / keyword / sort_by）
- `GET /api/stalls/{id}`
- `GET /api/stalls/{id}/reviews`

评论与个人行为：
- `POST /api/reviews`
- `GET/PUT/DELETE /api/users/me/reviews/{id}`
- `POST/DELETE /api/users/me/favorites/{stall_id}`
- `GET /api/users/me/favorites`
- `POST /api/users/me/blacklist`
- `DELETE /api/users/me/blacklist/{stall_id}`
- `GET /api/users/me/blacklist`
- `POST /api/users/me/history`
- `GET /api/users/me/history`

排行榜与推荐：
- `GET /api/rankings/score`
- `GET /api/rankings/hot`
- `GET /api/rankings/latest`
- `GET /api/recommendations/today`
- `POST /api/recommendations/feed`

管理员：
- `POST/PUT/DELETE /api/admin/stalls/{id}`
- `POST/PUT /api/admin/canteens/{id}`
- `GET/POST/PUT /api/admin/tags/{id}`
- `DELETE /api/admin/reviews/{id}`
- `GET /api/admin/users`
- `PUT /api/admin/users/{id}/role`
