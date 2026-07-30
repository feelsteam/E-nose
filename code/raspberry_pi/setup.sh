#!/bin/bash
# ============================================================
# SmellGood E-Nose Server — Raspberry Pi 一鍵安裝腳本
# 執行方式：
#   chmod +x setup.sh
#   ./setup.sh
# ============================================================

set -e

# ── 變數設定 ─────────────────────────────────────────────
DB_PASS="smellgood2026"
DB_NAME="smellgood"
APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PI_USER="$(whoami)"

echo "========================================"
echo "  SmellGood E-Nose — Raspberry Pi 安裝"
echo "  安裝路徑: $APP_DIR"
echo "  執行使用者: $PI_USER"
echo "========================================"

# ── 1. 系統更新 ───────────────────────────────────────────
echo ""
echo "[1/6] 更新系統套件..."
sudo apt update

# ── 2. 安裝 MariaDB ───────────────────────────────────────
echo ""
echo "[2/6] 安裝 MariaDB..."
sudo apt install -y mariadb-server mariadb-client
sudo systemctl start mariadb
sudo systemctl enable mariadb

# 設定 root 密碼（mysql_native_password 確保相容性）
sudo mysql -e "ALTER USER 'root'@'localhost' IDENTIFIED VIA mysql_native_password USING PASSWORD('${DB_PASS}');"
sudo mysql -u root -p"${DB_PASS}" -e "FLUSH PRIVILEGES;"
echo "  [OK] MariaDB 密碼設定完成（密碼: ${DB_PASS}）"

# ── 3. 建立資料庫與資料表 ─────────────────────────────────
echo ""
echo "[3/6] 建立 smellgood 資料庫..."
mysql -u root -p"${DB_PASS}" < "${APP_DIR}/setup_db.sql"
echo "  [OK] 資料庫與三張資料表建立完成"

# ── 4. 安裝 Apache2 + PHP ─────────────────────────────────
echo ""
echo "[4/6] 安裝 Apache2 + PHP..."
sudo apt install -y apache2 php php-mysql libapache2-mod-php
sudo systemctl enable apache2
sudo systemctl start apache2
echo "  [OK] Apache2 啟動完成"

# ── 5. 安裝 phpMyAdmin ────────────────────────────────────
echo ""
echo "[5/6] 安裝 phpMyAdmin..."

# 預先回答 debconf 問題，避免安裝中斷
echo "phpmyadmin phpmyadmin/dbconfig-install boolean true"              | sudo debconf-set-selections
echo "phpmyadmin phpmyadmin/app-password-confirm password ${DB_PASS}"   | sudo debconf-set-selections
echo "phpmyadmin phpmyadmin/mysql/admin-pass password ${DB_PASS}"       | sudo debconf-set-selections
echo "phpmyadmin phpmyadmin/mysql/app-pass password ${DB_PASS}"         | sudo debconf-set-selections
echo "phpmyadmin phpmyadmin/reconfigure-webserver multiselect apache2"  | sudo debconf-set-selections

sudo DEBIAN_FRONTEND=noninteractive apt install -y phpmyadmin

# 允許 root 帳號登入 phpMyAdmin
PMA_CONF="/etc/phpmyadmin/config.inc.php"
if [ -f "$PMA_CONF" ]; then
    if ! sudo grep -q "AllowRoot" "$PMA_CONF"; then
        echo "\$cfg['Servers'][\$i]['AllowRoot'] = true;" | sudo tee -a "$PMA_CONF" > /dev/null
        echo "  [OK] 已啟用 root 登入 phpMyAdmin"
    fi
fi

sudo systemctl restart apache2
echo "  [OK] phpMyAdmin 安裝完成"

# ── 6. 安裝 Python 套件 ───────────────────────────────────
echo ""
echo "[6/6] 安裝 Python 套件（建立虛擬環境）..."
sudo apt install -y python3-pip python3-venv
python3 -m venv "${APP_DIR}/venv"
"${APP_DIR}/venv/bin/pip" install --upgrade pip
"${APP_DIR}/venv/bin/pip" install -r "${APP_DIR}/requirements.txt"
echo "  [OK] Python 套件安裝完成"

# ── 建立 systemd 服務（開機自動啟動）────────────────────
echo ""
echo "[*] 設定 systemd 開機自動啟動..."
sudo tee /etc/systemd/system/smellgood.service > /dev/null <<EOF
[Unit]
Description=SmellGood E-Nose Server v3
After=network.target mariadb.service

[Service]
User=${PI_USER}
WorkingDirectory=${APP_DIR}
ExecStart=${APP_DIR}/venv/bin/python ${APP_DIR}/app_v3.py
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable smellgood
sudo systemctl start smellgood
echo "  [OK] 服務已啟動"

# ── 完成 ─────────────────────────────────────────────────
PI_IP=$(hostname -I | awk '{print $1}')
echo ""
echo "========================================"
echo "  安裝完成！"
echo "========================================"
echo "  網頁介面 :  http://${PI_IP}:5000"
echo "  phpMyAdmin: http://${PI_IP}/phpmyadmin"
echo "  DB 帳號   : root / ${DB_PASS}"
echo "========================================"
echo "  確認服務狀態: sudo systemctl status smellgood"
echo "  查看服務日誌: sudo journalctl -u smellgood -f"
echo "  重啟服務    : sudo systemctl restart smellgood"
echo "========================================"
