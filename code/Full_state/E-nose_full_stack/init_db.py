# ============================================================================
# init_db.py — 初始化 SmellGood MySQL 資料庫
# ============================================================================
# 執行方式：py init_db.py
# 前置條件：MySQL Server 已安裝並啟動
# ============================================================================

import mysql.connector

MYSQL_HOST = '127.0.0.1'
MYSQL_USER = 'root'
MYSQL_PASS = '00000000'
MYSQL_DB   = 'smellgood'


def init_database():
    """建立資料庫與三張資料表"""
    
    # 先連接 MySQL（不指定資料庫）
    print("[INFO] 正在連接 MySQL...")
    db = mysql.connector.connect(
        host=MYSQL_HOST,
        user=MYSQL_USER,
        password=MYSQL_PASS
    )
    cursor = db.cursor()

    # 建立資料庫
    cursor.execute(f"CREATE DATABASE IF NOT EXISTS `{MYSQL_DB}` "
                   f"DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
    cursor.execute(f"USE `{MYSQL_DB}`")
    print(f"[OK] 資料庫 '{MYSQL_DB}' 已就緒")

    # ── 表 1：human（受試者基本資料）──
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS `human` (
            `subject_id`   VARCHAR(32)  NOT NULL  PRIMARY KEY  COMMENT '受試者編號',
            `created_at`   DATETIME     NOT NULL  DEFAULT CURRENT_TIMESTAMP  COMMENT '建檔時間',
            `gender`       ENUM('Male','Female')  NOT NULL  COMMENT '性別',
            `age`          INT          NOT NULL  COMMENT '年齡',
            `bmi`          FLOAT        NOT NULL  COMMENT 'BMI'
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='受試者基本資料'
    """)
    print("[OK] 表 'human' 已就緒")

    # ── 表 2：value（量測數據）──
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS `value` (
            `data_id`      INT          NOT NULL  AUTO_INCREMENT  PRIMARY KEY  COMMENT '資料編號',
            `subject_id`   VARCHAR(32)  NOT NULL  COMMENT '對應的受試者ID',
            `uploaded_at`  DATETIME     NOT NULL  DEFAULT CURRENT_TIMESTAMP  COMMENT '上傳時間',
            `acetone_ppm`  FLOAT        NOT NULL  COMMENT '丙酮濃度 (ppm)',
            `meal_state`   VARCHAR(32)  NOT NULL  COMMENT '進食狀態 (fasting / postmeal_Xh)',
            FOREIGN KEY (`subject_id`) REFERENCES `human`(`subject_id`)
                ON UPDATE CASCADE ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='量測數據'
    """)
    print("[OK] 表 'value' 已就緒")

    # ── 表 3：ans（預測結果）──
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS `ans` (
            `result_id`     INT          NOT NULL  AUTO_INCREMENT  PRIMARY KEY  COMMENT '結果編號',
            `subject_id`    VARCHAR(32)  NOT NULL  COMMENT '對應的受試者ID',
            `uploaded_at`   DATETIME     NOT NULL  DEFAULT CURRENT_TIMESTAMP  COMMENT '結果上傳時間',
            `prediction`    INT          NOT NULL  COMMENT '判斷結果 (0=健康, 1=糖尿病)',
            `confidence`    FLOAT        NOT NULL  COMMENT '信心度 (0.0~1.0)',
            `acetone_max`   FLOAT        NOT NULL  COMMENT '丙酮最大值 (ppm)',
            `acetone_slope` FLOAT        NOT NULL  COMMENT '丙酮斜率 (ppm/min)',
            FOREIGN KEY (`subject_id`) REFERENCES `human`(`subject_id`)
                ON UPDATE CASCADE ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='預測結果'
    """)
    print("[OK] 表 'ans' 已就緒")

    db.commit()
    cursor.close()
    db.close()

    print()
    print("=" * 50)
    print("  SmellGood 資料庫初始化完成！")
    print("  資料庫: " + MYSQL_DB)
    print("  資料表: human, value, ans")
    print("=" * 50)


if __name__ == '__main__':
    try:
        init_database()
    except mysql.connector.Error as e:
        print(f"[ERROR] MySQL 錯誤: {e}")
        print("[HINT] 請確認 MySQL Server 已啟動，且帳密正確。")
    except Exception as e:
        print(f"[ERROR] {e}")
