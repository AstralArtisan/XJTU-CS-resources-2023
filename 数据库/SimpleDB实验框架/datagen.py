import csv
import os
import random

DATA_DIR = "data"
if not os.path.exists(DATA_DIR):
    os.makedirs(DATA_DIR)

# 配置
NUM_STUDENTS = 500
NUM_COURSES = 50
NUM_SCORES = 5000


def generate_data():
    print(f"Generating data in '{DATA_DIR}'...")

    # 1. 生成 Student (id, name, age)
    student_ids = []
    with open(f"{DATA_DIR}/student.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["id", "name", "age"])
        for i in range(1, NUM_STUDENTS + 1):
            writer.writerow([i, f"Stu_{i}", random.randint(18, 25)])
            student_ids.append(i)
    print(f"  - student.csv ({NUM_STUDENTS} rows)")

    # 2. 生成 Course (id, title, credits)
    course_ids = []
    with open(f"{DATA_DIR}/course.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["id", "title", "credits"])
        for i in range(1, NUM_COURSES + 1):
            title = f"Course_{i}_" + random.choice(["Math", "CS", "Art", "Hist", "Phys"])
            writer.writerow([i, title, random.randint(1, 4)])
            course_ids.append(i)
    print(f"  - course.csv ({NUM_COURSES} rows)")

    # 3. 生成 Score (sid, student_id, course_id, grade)
    # 确保关联键 student_id 和 course_id 是存在的
    with open(f"{DATA_DIR}/score.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["sid", "student_id", "course_id", "grade"])
        for i in range(1, NUM_SCORES + 1):
            s_id = random.choice(student_ids)
            c_id = random.choice(course_ids)
            grade = random.randint(60, 100)
            writer.writerow([i, s_id, c_id, grade])
    print(f"  - score.csv ({NUM_SCORES} rows)")

    print("Done!")


if __name__ == "__main__":
    generate_data()
