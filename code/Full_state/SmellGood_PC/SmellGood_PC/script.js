function processData() {
    const userId = document.getElementById('userId').value.trim();
    const gender = document.getElementById('gender').value;
    const age = parseInt(document.getElementById('age').value);
    const height = parseFloat(document.getElementById('height').value);
    const weight = parseFloat(document.getElementById('weight').value);
    const fre = parseInt(document.getElementById('fre').value);
    
    const inputMsg = document.getElementById('input-msg');

    if (!userId || isNaN(age) || isNaN(height) || isNaN(weight) || isNaN(fre)) {
        inputMsg.style.color = '#f87171';
        inputMsg.textContent = "Please fill in all fields.";
        return;
    }

    inputMsg.style.color = '#94a3b8';
    inputMsg.textContent = "Processing...";

    const bmi = weight / Math.pow((height / 100), 2);

    const now = new Date();
    const currentTime = now.getFullYear() + '-' + 
                        String(now.getMonth() + 1).padStart(2, '0') + '-' + 
                        String(now.getDate()).padStart(2, '0') + ' ' + 
                        String(now.getHours()).padStart(2, '0') + ':' + 
                        String(now.getMinutes()).padStart(2, '0') + ':' + 
                        String(now.getSeconds()).padStart(2, '0');

    const payload = {
        userId: userId,
        gender: gender,
        age: age,
        bmi: bmi,
        fre: fre,
        time: currentTime,
        acetone: 0.0,
        slope: 0.0,
        state: "Waiting"
    };

    fetch('http://172.20.10.4:5000/api/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => response.json())
    .then(data => {
        if(data.status === "success") {
            showOutputScreen(bmi.toFixed(2), currentTime);
        } else {
            inputMsg.style.color = '#f87171';
            inputMsg.textContent = "[Error] " + data.message;
        }
    })
    .catch(error => {
        console.error('Error:', error);
        inputMsg.style.color = '#f87171';
        inputMsg.textContent = "Connection failed. Is app.py running?";
    });
}

function showOutputScreen(bmiValue, timeString) {
    document.getElementById('input-screen').classList.remove('active');
    document.getElementById('output-screen').classList.add('active');
    
    document.getElementById('display-bmi').textContent = bmiValue;
    document.getElementById('display-time').textContent = "Record Time: " + timeString;
    
    const outputMsg = document.getElementById('output-msg');
    outputMsg.style.color = '#4ade80';
    outputMsg.textContent = "Data successfully saved to database.";
}

function resetUI() {
    document.getElementById('output-screen').classList.remove('active');
    document.getElementById('input-screen').classList.add('active');
    
    document.getElementById('sensorForm').reset();
    document.getElementById('input-msg').textContent = "";
}

// 🚀 控制加載層淡出 (使用更穩定的 DOMContentLoaded)
document.addEventListener('DOMContentLoaded', function() {
    const loadingOverlay = document.getElementById('loading-overlay');
    
    if (loadingOverlay) {
        // 設定 2 秒後加載畫面淡出
        setTimeout(function() {
            loadingOverlay.classList.add('hidden');
        }, 2000);
    }
});