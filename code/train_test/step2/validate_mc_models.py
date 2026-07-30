import pandas as pd
import numpy as np
import os
import sys
import matplotlib.pyplot as plt
from sklearn.metrics import accuracy_score, confusion_matrix, recall_score
import joblib

# 忽略警告
import warnings
warnings.filterwarnings('ignore')

# 讓 Python 能夠存取上一層 step1 中的 Stage1RiskCalculator
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
step1_dir = os.path.join(parent_dir, 'step1')
sys.path.append(step1_dir)

from train_stage1 import Stage1RiskCalculator

def calculate_specificity(y_true, y_pred):
    cm = confusion_matrix(y_true, y_pred)
    if cm.shape != (2, 2):
        return 0.0
    tn, fp, fn, tp = cm.ravel()
    return tn / (tn + fp)

def generate_cm_plot(y_true, y_pred, title, save_path):
    cm = confusion_matrix(y_true, y_pred)
    acc = accuracy_score(y_true, y_pred)
    rec = recall_score(y_true, y_pred)
    spec = calculate_specificity(y_true, y_pred)
    
    plt.figure(figsize=(6, 5))
    fig, ax = plt.subplots(figsize=(6, 5))
    cax = ax.matshow(cm, cmap=plt.cm.Blues, alpha=0.8)
    plt.colorbar(cax)
    
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            ax.text(x=j, y=i, s=cm[i, j], va='center', ha='center', size=16,
                    color="white" if cm[i, j] > cm.max()/2 else "black")
            
    ax.set_xticks([0, 1])
    ax.set_yticks([0, 1])
    ax.set_xticklabels(["Healthy (0)", "Diabetes (1)"])
    ax.set_yticklabels(["Healthy (0)", "Diabetes (1)"])
    ax.xaxis.set_ticks_position('bottom')
    
    plt.xlabel('Predicted Labels', fontsize=12)
    plt.ylabel('True Labels', fontsize=12)
    plt.title(f"{title}\nAccuracy={acc:.2f}, Recall={rec:.2f}, Specificity={spec:.2f}", fontsize=14)
    plt.tight_layout()
    plt.savefig(save_path, dpi=300)
    plt.close('all')

def main():
    # 對應你要讀取的三個檔案位置
    data_path = os.path.join(parent_dir, 'extracted_data.csv')
    rf_model_path = os.path.join(step1_dir, 'stage2_rf_model_v2.pkl')
    dt_model_path = os.path.join(step1_dir, 'stage2_dt_model_v2.pkl')
    
    print(f">>> 載入真實驗證資料: {data_path}")
    df = pd.read_csv(data_path)
    
    # 建立模型所需的特徵轉換
    print(">>> 準備資料轉換以對齊蒙地卡羅大模型...")
    df['Age'] = df['年紀']
    df['Gender'] = df['性別'].map({'M': 1, 'F': 0})
    df['BMI'] = df['BMI']
    df['Acetone_Fasting'] = df['空腹狀態丙酮']
    df['Acetone_2h'] = df['飯後丙酮']
    df['Acetone_Slope'] = df['丙酮變化斜率']
    
    # ✨新增 V2 關聯特徵：計算丙酮最大值
    print(">>> 結合空腹與飯後丙酮為「丙酮最大值」(Acetone_Max)...")
    df['Acetone_Max'] = df[['Acetone_Fasting', 'Acetone_2h']].max(axis=1)
    
    print(">>> 呼叫 Stage1RiskCalculator 計算第一階危險值 (Risk_Value)...")
    risk_calc = Stage1RiskCalculator()
    risks = []
    for _, row in df.iterrows():
        r = risk_calc.predict_risk_single(row['Age'], row['BMI'], row['Gender'])
        risks.append(r)
    df['Risk_Value'] = risks
    
    # ✨ V2 的新特徵排列順序 (嚴格剔除 Age, Gender, BMI 等特徵以防錯誤)
    expected_features = [
        'Risk_Value', 
        'Acetone_Fasting', 'Acetone_2h', 'Acetone_Slope', 'Acetone_Max'
    ]
    
    X_test = df[expected_features]
    y_true = df['糖尿病'].values
    
    print(f"\n驗證資料總數: {len(X_test)} 筆\n")

    # ==========================
    # 測試 1: Random Forest
    # ==========================
    if os.path.exists(rf_model_path):
        print(">>> 測試 隨機森林模型 V2 (stage2_rf_model_v2.pkl)...")
        rf_model = joblib.load(rf_model_path)
        y_pred_rf = rf_model.predict(X_test)
        
        rf_cm_path = os.path.join(current_dir, 'validate_rf_v2_confusion_matrix.png')
        generate_cm_plot(y_true, y_pred_rf, "Monte Carlo RF V2 vs True Data", rf_cm_path)
        print(f"    -> [儲存] 隨機森林混淆矩陣: {rf_cm_path}")
    else:
        print(f"錯誤: 找不到隨機森林模型檔 ({rf_model_path})")
        
    # ==========================
    # 測試 2: Decision Tree
    # ==========================
    if os.path.exists(dt_model_path):
        print("\n>>> 測試 決策樹模型 V2 (stage2_dt_model_v2.pkl)...")
        dt_model = joblib.load(dt_model_path)
        y_pred_dt = dt_model.predict(X_test)
        
        dt_cm_path = os.path.join(current_dir, 'validate_dt_v2_confusion_matrix.png')
        generate_cm_plot(y_true, y_pred_dt, "Monte Carlo DT V2 vs True Data", dt_cm_path)
        print(f"    -> [儲存] 決策樹混淆矩陣: {dt_cm_path}")
    else:
        print(f"錯誤: 找不到決策樹模型檔 ({dt_model_path})")

    print("\n✅ 所有真實資料用 V2 模型驗證完畢！")

if __name__ == '__main__':
    main()
