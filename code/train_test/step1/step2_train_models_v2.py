import pandas as pd
import numpy as np
from sklearn.tree import DecisionTreeClassifier, plot_tree
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score
import joblib
import matplotlib.pyplot as plt
import os
import sys

# 忽略警告
import warnings
warnings.filterwarnings('ignore')

# 匯入第一階段查表模組以計算危險值 (必須與 train_stage1.py 同目錄)
from train_stage1 import Stage1RiskCalculator

def generate_truncated_normal(mean, std, lower, upper, size):
    """產生限制在下限與上限之間的隨機常態分佈"""
    samples = []
    while len(samples) < size:
        s = np.random.normal(mean, std, size * 2)
        valid = s[(s >= lower) & (s <= upper)]
        samples.extend(valid.tolist())
    return np.array(samples[:size])

def generate_synthetic_data():
    print(">>> [Step 1] 開始進行蒙地卡羅模擬合成虛擬病患資料...")
    
    # 這是你之前經過修改輸入的真實統計值！
    stats_healthy = {
        'Age': (46.0, 12.8, 20.0, 73.0),
        'BMI': (22.1, 3.1, 17.1, 33.9),
        'Acetone_Fasting': (1.3, 0.3, 0.3, 1.9),
        'Acetone_2h': (1.0, 0.6, 0.1, 2.0)
    }
    
    stats_t2_diabetes = {
        'Age': (46.0, 12.9, 20.0, 73.0),
        'BMI': (26.5, 3.8, 17.9, 38.2),
        'Acetone_Fasting': (1.7, 0.7, 0.1, 19.8),
        'Acetone_2h': (1.5, 1.3, 0.1, 10.6)
    }
    
    num_healthy = 50
    num_t2_diabetes = 300
    
    np.random.seed(42)  # 保持生成資料一定程度上的可重現性
    fake_data = []
    
    # 1. 生成健康病患 (Diagnosis = 0)
    for _ in range(num_healthy):
        fake_data.append({
            'Age': generate_truncated_normal(*stats_healthy['Age'], 1)[0],
            'Gender': np.random.choice([0, 1]),
            'BMI': generate_truncated_normal(*stats_healthy['BMI'], 1)[0],
            'Acetone_Fasting': generate_truncated_normal(*stats_healthy['Acetone_Fasting'], 1)[0],
            'Acetone_2h': generate_truncated_normal(*stats_healthy['Acetone_2h'], 1)[0],
            'Diagnosis': 0
        })
        
    # 2. 生成第二型糖尿病患 (Diagnosis = 1)
    for _ in range(num_t2_diabetes):
        fake_data.append({
            'Age': generate_truncated_normal(*stats_t2_diabetes['Age'], 1)[0],
            'Gender': np.random.choice([0, 1]),
            'BMI': generate_truncated_normal(*stats_t2_diabetes['BMI'], 1)[0],
            'Acetone_Fasting': generate_truncated_normal(*stats_t2_diabetes['Acetone_Fasting'], 1)[0],
            'Acetone_2h': generate_truncated_normal(*stats_t2_diabetes['Acetone_2h'], 1)[0],
            'Diagnosis': 1
        })
        
    df_synthetic = pd.DataFrame(fake_data)
    print(f">>> 合成資料生成完畢，共 {len(df_synthetic)} 筆。\n")
    return df_synthetic

def feature_engineering(df):
    print(">>> [Step 2] 載入 Phase 1 查表法模組，萃取 Risk_Value 與 新增特徵...")
    
    # 初始化你的查表計算法
    risk_calc = Stage1RiskCalculator() 
    
    # 1. 計算每個病患的基礎危險值 (Risk_Value) - 雖不用於最終輸入，但仍然可供檢視
    risks = []
    for _, row in df.iterrows():
        r = risk_calc.predict_risk_single(row['Age'], row['BMI'], row['Gender'])
        risks.append(r)
    df['Risk_Value'] = risks
    
    # 2. 計算丙酮變化斜率 (Slope / 分鐘)
    df['Acetone_Slope'] = (df['Acetone_2h'] - df['Acetone_Fasting']) / 120.0
    
    # 3. ✨新增：計算丙酮最大值
    df['Acetone_Max'] = df[['Acetone_Fasting', 'Acetone_2h']].max(axis=1)
    
    # 儲存供檢視用
    df.to_csv('synthetic_stage2_data_v2.csv', index=False)
    print(">>> 特徵萃取完畢！已產出 'synthetic_stage2_data_v2.csv'\n")
    return df

def train_models(df):
    # =========================================================
    # ✨本次 V2 訓練特徵：去除 Age, BMI, Gender 防止冗餘，並加入 Acetone_Max
    # =========================================================
    train_features = [
        'Risk_Value', 
        'Acetone_Fasting', 'Acetone_2h', 'Acetone_Slope', 'Acetone_Max'
    ]
    target = 'Diagnosis'
    
    X = df[train_features]
    y = df[target]
    
    print(f"訓練用特徵: {train_features}")
    
    # --- 1. 訓練單一決策樹 (Decision Tree) ---
    print("\n=== 決策樹 (Decision Tree) 訓練結果 ===")
    dt = DecisionTreeClassifier(max_depth=3, random_state=42, class_weight='balanced')
    dt.fit(X, y)
    
    print(f"訓練表現 (Accuracy): {accuracy_score(y, dt.predict(X)):.4f}")
    
    # 畫出決策樹圖表
    plt.figure(figsize=(16, 8))
    plot_tree(dt, feature_names=train_features, class_names=['Healthy', 'T2 Diabetes'], 
              filled=True, rounded=True, proportion=False, fontsize=10)
    plt.title("Stage 2 Decision Tree V2 (Medical E-Nose)", fontsize=18)
    plt.tight_layout()
    plt.savefig('stage2_decision_tree_v2.png', dpi=300)
    joblib.dump(dt, 'stage2_dt_model_v2.pkl')
    print(">>> [儲存] 決策流程圖 => stage2_decision_tree_v2.png")
    
    # --- 2. 訓練隨機森林 (Random Forest) ---
    print("\n=== 隨機森林 (Random Forest) 訓練結果 ===")
    rf = RandomForestClassifier(n_estimators=100, max_depth=None, random_state=42, class_weight='balanced', n_jobs=-1)
    rf.fit(X, y)
    
    print(f"訓練表現 (Accuracy): {accuracy_score(y, rf.predict(X)):.4f}")
    
    # 提取隨機森林特徵重要性
    importances = rf.feature_importances_
    feat_imp = pd.DataFrame({'Feature': train_features, 'Importance': importances}).sort_values(by='Importance', ascending=False)
    
    print("\n 特徵重要性 (Feature Importance) 排名:")
    print(feat_imp.to_string(index=False))
    
    # 輸出重要性長條圖
    import seaborn as sns
    plt.figure(figsize=(8, 5))
    sns.barplot(x='Importance', y='Feature', data=feat_imp, palette='viridis')
    plt.title('Random Forest Feature Importance V2')
    plt.tight_layout()
    plt.savefig('stage2_rf_importance_v2.png', dpi=300)
    joblib.dump(rf, 'stage2_rf_model_v2.pkl')
    print(">>> [儲存] 特徵重要性圖 => stage2_rf_importance_v2.png\n")

if __name__ == '__main__':
    # 1. 產生初始合成特徵
    df_syn = generate_synthetic_data()
    # 2. 加入危險值與斜率特徵
    df_syn = feature_engineering(df_syn)
    # 3. 訓練決策樹與隨機森林
    train_models(df_syn)
