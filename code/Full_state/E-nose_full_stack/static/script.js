// ============================================================================
// SmellGood E-Nose — Frontend Script
// ============================================================================

let pollInterval = null;
let currentTaskId = null;

// ============================================================================
// 進食狀態切換
// ============================================================================
document.querySelectorAll('input[name="mealState"]').forEach(radio => {
    radio.addEventListener('change', (e) => {
        const postmealInput = document.getElementById('postmeal-time');
        if (e.target.value === 'postmeal') {
            postmealInput.classList.remove('hidden');
        } else {
            postmealInput.classList.add('hidden');
        }
    });
});

// ============================================================================
// 啟動檢測
// ============================================================================
async function startDetection() {
    const userId  = document.getElementById('userId').value.trim();
    const gender  = document.getElementById('gender').value;
    const age     = parseInt(document.getElementById('age').value);
    const height  = parseFloat(document.getElementById('height').value);
    const weight  = parseFloat(document.getElementById('weight').value);
    const mealRadio = document.querySelector('input[name="mealState"]:checked').value;

    const inputMsg = document.getElementById('input-msg');

    // 驗證
    if (!userId || isNaN(age) || isNaN(height) || isNaN(weight)) {
        inputMsg.style.color = '#f87171';
        inputMsg.textContent = '⚠ 請填寫所有必要欄位。';
        return;
    }
    if (age <= 0 || height <= 0 || weight <= 0) {
        inputMsg.style.color = '#f87171';
        inputMsg.textContent = '⚠ 數值必須大於零。';
        return;
    }

    // 組合進食狀態
    let mealState = mealRadio;
    if (mealRadio === 'postmeal') {
        const hours = parseFloat(document.getElementById('mealHours').value) || 2;
        mealState = 'postmeal_' + hours + 'h';
    }

    // 禁用按鈕
    const btn = document.getElementById('startBtn');
    btn.disabled = true;
    inputMsg.style.color = '#94a3b8';
    inputMsg.textContent = '正在連接伺服器...';

    try {
        const response = await fetch('/api/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                subjectId: userId,
                gender: gender,
                age: age,
                height: height,
                weight: weight,
                mealState: mealState
            })
        });

        const data = await response.json();

        if (data.status === 'success') {
            currentTaskId = data.taskId;

            // 顯示 BMI
            const bmiDisplay = document.getElementById('bmi-display');
            bmiDisplay.textContent = 'BMI: ' + data.bmi.toFixed(2);

            showScreen('measuring-screen');
            startPolling();
        } else {
            inputMsg.style.color = '#f87171';
            inputMsg.textContent = '❌ ' + (data.message || '發生錯誤');
            btn.disabled = false;
        }
    } catch (error) {
        console.error('Connection error:', error);
        inputMsg.style.color = '#f87171';
        inputMsg.textContent = '❌ 無法連接伺服器，請確認 app.py 已啟動。';
        btn.disabled = false;
    }
}

// ============================================================================
// 輪詢結果
// ============================================================================
function startPolling() {
    const statusEl = document.getElementById('measure-status');

    pollInterval = setInterval(async () => {
        try {
            const response = await fetch('/api/result/' + currentTaskId);
            const data = await response.json();

            // 更新裝置狀態文字
            if (data.deviceStatus) {
                const statusMap = {
                    'waiting_for_device': '⏳ 等待 BMduino 量測完成...',
                    'pending':      '⏳ 等待裝置回應...',
                    'stabilizing':  '🔬 感測器穩定中...',
                    'ready':        '💨 請對著感測器吹氣...',
                    'measuring':    '📊 正在量測呼吸樣本...',
                    'computing':    '🧮 正在分析數據...',
                    'predicting':   '🧠 正在進行糖尿病預測...'
                };
                statusEl.textContent = statusMap[data.deviceStatus] || data.deviceStatus;
            }

            // 檢查結果
            if (data.status === 'done' && data.result) {
                stopPolling();
                showResult(data.result);
            } else if (data.status === 'error') {
                stopPolling();
                statusEl.textContent = '❌ 錯誤: ' + (data.result && data.result.error ? data.result.error : '未知錯誤');
            } else if (data.status === 'timeout') {
                stopPolling();
                statusEl.textContent = '⏰ 量測逾時，請返回重試。';
            }
        } catch (error) {
            console.error('Polling error:', error);
        }
    }, 2000);
}

function stopPolling() {
    if (pollInterval) {
        clearInterval(pollInterval);
        pollInterval = null;
    }
}

// ============================================================================
// 顯示結果
// ============================================================================
function showResult(result) {
    const now = new Date();
    const timeStr = now.getFullYear() + '-' +
        String(now.getMonth() + 1).padStart(2, '0') + '-' +
        String(now.getDate()).padStart(2, '0') + ' ' +
        String(now.getHours()).padStart(2, '0') + ':' +
        String(now.getMinutes()).padStart(2, '0') + ':' +
        String(now.getSeconds()).padStart(2, '0');

    if (result.type === 'simple') {
        // ── 簡易結果（首次量測） ──
        document.getElementById('simple-ppm').textContent = result.acetone_ppm.toFixed(3);
        document.getElementById('simple-time').textContent = '記錄時間: ' + timeStr;
        showScreen('simple-result-screen');

    } else if (result.type === 'detailed') {
        // ── 詳細結果（含預測） ──
        const card = document.getElementById('prediction-card');
        const predText = document.getElementById('prediction-text');
        const confText = document.getElementById('confidence-text');
        const confFill = document.getElementById('confidence-fill');

        // 設定預測結果樣式
        card.classList.remove('healthy', 'diabetes');
        if (result.prediction === 0) {
            predText.textContent = '健康 HEALTHY';
            card.classList.add('healthy');
        } else {
            predText.textContent = '第二型糖尿病 T2 DIABETES';
            card.classList.add('diabetes');
        }

        // 信心度動畫
        const confPercent = (result.confidence * 100).toFixed(1);
        confText.textContent = '信心度 Confidence: ' + confPercent + '%';
        // 延遲觸發以啟動 CSS transition
        setTimeout(() => {
            confFill.style.width = confPercent + '%';
        }, 100);

        // 詳細數值
        document.getElementById('detail-ppm').textContent   = result.acetone_ppm.toFixed(3) + ' ppm';
        document.getElementById('detail-max').textContent   = result.acetone_max.toFixed(3) + ' ppm';
        document.getElementById('detail-slope').textContent = result.acetone_slope.toFixed(6) + ' ppm/min';

        document.getElementById('detailed-time').textContent = '記錄時間: ' + timeStr;
        showScreen('detailed-result-screen');
    }
}

// ============================================================================
// 畫面切換
// ============================================================================
function showScreen(screenId) {
    document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
    document.getElementById(screenId).classList.add('active');
}

function resetUI() {
    stopPolling();
    currentTaskId = null;

    // 重設表單
    document.getElementById('sensorForm').reset();
    document.getElementById('input-msg').textContent = '';
    document.getElementById('startBtn').disabled = false;
    document.getElementById('postmeal-time').classList.add('hidden');

    // 重設結果顯示
    document.getElementById('confidence-fill').style.width = '0%';

    showScreen('input-screen');
}

// ============================================================================
// 初始化：Loading 畫面 + 裝置狀態檢查
// ============================================================================
document.addEventListener('DOMContentLoaded', () => {
    const loading = document.getElementById('loading-overlay');
    const badge   = document.getElementById('device-badge');
    const badgeText = document.getElementById('device-status-text');

    // 檢查伺服器狀態
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            if (data.device === 'connected') {
                badge.classList.add('connected');
                badge.classList.remove('disconnected');
                badgeText.textContent = '裝置已連線';
            } else {
                badge.classList.add('disconnected');
                badge.classList.remove('connected');
                badgeText.textContent = '裝置未連線';
            }
            setTimeout(() => { loading.classList.add('hidden'); }, 1200);
        })
        .catch(() => {
            badge.classList.add('disconnected');
            badgeText.textContent = '伺服器離線';
            setTimeout(() => { loading.classList.add('hidden'); }, 1500);
        });

    // 定期更新裝置狀態
    setInterval(updateDeviceStatus, 10000);
});

function updateDeviceStatus() {
    const badge     = document.getElementById('device-badge');
    const badgeText = document.getElementById('device-status-text');

    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            if (data.device === 'connected') {
                badge.classList.add('connected');
                badge.classList.remove('disconnected');
                badgeText.textContent = '裝置已連線';
            } else {
                badge.classList.remove('connected');
                badge.classList.add('disconnected');
                badgeText.textContent = '裝置未連線';
            }
        })
        .catch(() => {
            badge.classList.remove('connected');
            badge.classList.add('disconnected');
            badgeText.textContent = '伺服器離線';
        });
}
