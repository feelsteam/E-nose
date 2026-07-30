import pandas as pd
import numpy as np
import os
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.tree import DecisionTreeClassifier, plot_tree
from sklearn.metrics import accuracy_score
import joblib

# 忽略警告
import warnings
warnings.filterwarnings('ignore')

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)

def main():
    # 資料路徑
    data_path = os.path.join(parent_dir, 'extracted_data.csv')
    print(f">>> 載入真實資料: {data_path}")
    
    # 讀取資料
    df = pd.read_csv(data_path)
    
    # === 特徵工程: 丙酮處理 ===
    # 將空腹與飯後丙酮合併，取其中最大值
    print(">>> 結合空腹與飯後丙酮為「丙酮最大值」(Acetone_Max)...")
    df['Acetone_Max'] = df[['空腹狀態丙酮', '飯後丙酮']].max(axis=1)
    
    # 斜率直接拿現成的欄位
    df['Acetone_Slope'] = df['丙酮變化斜率']
    df['Diagnosis'] = df['糖尿病']
    
    # 依指示移除 Risk_Value，確認最終的兩個輸入特徵
    train_features = ['Acetone_Max', 'Acetone_Slope']
    target = 'Diagnosis'
    
    X = df[train_features]
    y = df[target]
    
    print(f"\n訓練樣本總數: {len(X)} 筆 (健康: {sum(y==0)}, 糖尿病: {sum(y==1)})")
    print(f"訓練用特徵: {train_features}")
    
    # === 決策樹訓練 ===
    print("\n=== 決策樹 (Decision Tree) 訓練開始 ===")
    dt = DecisionTreeClassifier(max_depth=5, random_state=42, class_weight='balanced')
    dt.fit(X, y)
    
    print(f"訓練表現 (Accuracy): {accuracy_score(y, dt.predict(X)):.4f}")
    
    # === 視覺化決策樹圖表 ===
    plt.figure(figsize=(10, 6))
    
    plot_tree(dt, feature_names=train_features, class_names=['Healthy', 'Diabetes'], 
              filled=True, rounded=True, proportion=False, fontsize=12)
    plt.title("Stage 2 Decision Tree (Max Depth = 5)", fontsize=16)
    plt.tight_layout()
    
    # 儲存於 step2 資料夾
    tree_plot_path = os.path.join(current_dir, 'stage2_decision_tree_extracted.png')
    plt.savefig(tree_plot_path, dpi=300)
    print(f">>> [儲存] 決策流程圖 => {tree_plot_path}")
    
    # === 生成並輸出特徵重要性 (Feature Importance) ===
    importances = dt.feature_importances_
    feat_imp = pd.DataFrame({'Feature': train_features, 'Importance': importances}).sort_values(by='Importance', ascending=False)
    
    print("\n 特徵重要性 (Feature Importance) 排名:")
    print(feat_imp.to_string(index=False))
    
    plt.figure(figsize=(8, 3))
    sns.barplot(x='Importance', y='Feature', data=feat_imp, palette='viridis')
    plt.title('Decision Tree Feature Importance (Acetone Only)')
    plt.tight_layout()
    
    imp_plot_path = os.path.join(current_dir, 'stage2_feature_importance.png')
    plt.savefig(imp_plot_path, dpi=300)
    print(f">>> [儲存] 特徵重要性排序圖 => {imp_plot_path}")

    # 儲存模型
    model_path = os.path.join(current_dir, 'stage2_dt_model.pkl')
    joblib.dump(dt, model_path)
    print(f">>> [儲存] 決策樹模型 => {model_path}")

if __name__ == '__main__':
    main()
