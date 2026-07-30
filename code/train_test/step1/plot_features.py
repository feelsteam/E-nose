import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import os
import warnings

warnings.filterwarnings("ignore")

# 設定中文字體
plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'Microsoft YaHei', 'sans-serif']
plt.rcParams['axes.unicode_minus'] = False

def calculate_bin_ratio(df, bin_col, target_col='Diagnosis'):
    # 計算該區間的總人數
    total_counts = df.groupby(bin_col).size()
    # 計算該區間內「有糖尿病」的人數
    diabetic_counts = df[df[target_col] == 1].groupby(bin_col).size()
    
    # 確保所有區間無論如何都有 0 作為預設值
    diabetic_counts = diabetic_counts.reindex(df[bin_col].cat.categories, fill_value=0)
    total_counts = total_counts.reindex(df[bin_col].cat.categories, fill_value=0)
    
    # 比率 = 該區間有糖尿病的人數 / 該區間總人數
    ratio = diabetic_counts / total_counts.replace(0, np.nan)
    ratio = ratio.fillna(0)
    return ratio

def main():
    print(">>> 載入 Dataset 1 (data1879.csv)...")
    data_path = '../data1879.csv'
    if not os.path.exists(data_path):
        data_path = 'data1879.csv'
        if not os.path.exists(data_path):
            print(f"錯誤: 找不到文件 {data_path}")
            return

    df = pd.read_csv(data_path)
    df = df.dropna(subset=['Age', 'BMI', 'Gender', 'Diagnosis'])
    total_samples = len(df)
    
    print(f">>> 實際總資料筆數: {total_samples}")
    
    # 1. 準備年齡區間 (Age Bin)
    age_bins = [0, 20, 30, 40, 50, 60, 70, 80, 90, 150]
    age_labels = ['<20', '21-30', '31-40', '41-50', '51-60', '61-70', '71-80', '81-90', '>90']
    df['Age_Bin'] = pd.cut(df['Age'], bins=age_bins, labels=age_labels, right=True)
    
    # 2. 準備 BMI 區間 (BMI Bin)
    # 細分區間以觀察關聯性
    bmi_bins = [0, 13, 16, 18.5, 21, 24, 27, 30, 35, 40, 50]
    bmi_labels = ['<13', '13-16', '16-18.5', '18.5-21', '21-24', '24-27', '27-30', '30-35', '35-40', '>40']
    df['BMI_Bin'] = pd.cut(df['BMI'], bins=bmi_bins, labels=bmi_labels, right=True)
    
    # 3. 計算比率
    age_ratio = calculate_bin_ratio(df, 'Age_Bin')
    bmi_ratio = calculate_bin_ratio(df, 'BMI_Bin')
    
    # 4. 性別直接計算
    gender_diabetic_counts = df[df['Diagnosis'] == 1].groupby('Gender').size()
    gender_total_counts = df.groupby('Gender').size()
    gender_ratio = {
        'Female (0)': gender_diabetic_counts.get(0, 0) / gender_total_counts.get(0, 1),
        'Male (1)':   gender_diabetic_counts.get(1, 0) / gender_total_counts.get(1, 1)
    }
    
    # 建立畫布與子圖 (1x3)
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    
    # --- 年齡子圖 ---
    sns.barplot(x=age_ratio.index, y=age_ratio.values, ax=axes[0], palette='Blues_r')
    axes[0].set_title('各年齡層之罹病率\n(該年齡段糖尿病患 / 該年齡段總人數)', fontsize=14)
    axes[0].set_xlabel('年齡區間', fontsize=12)
    axes[0].set_ylabel('比率 (Ratio)', fontsize=12)
    axes[0].set_xticklabels(axes[0].get_xticklabels(), rotation=45)
    # 在柱上方標註數值
    for i, v in enumerate(age_ratio.values):
        if v > 0:
            axes[0].text(i, v + 0.002, f'{v:.3f}', ha='center', fontsize=10, color='darkblue')
            
    # --- BMI 子圖 ---
    sns.barplot(x=bmi_ratio.index, y=bmi_ratio.values, ax=axes[1], palette='Oranges_r')
    axes[1].set_title('各種 BMI 區間之罹病率\n(該BMI區間糖尿病患 / 該BMI區間總人數)', fontsize=14)
    axes[1].set_xlabel('BMI 區間', fontsize=12)
    axes[1].set_ylabel('比率 (Ratio)', fontsize=12)
    axes[1].set_xticklabels(axes[1].get_xticklabels(), rotation=45)
    for i, v in enumerate(bmi_ratio.values):
        if v > 0:
            axes[1].text(i, v + 0.002, f'{v:.3f}', ha='center', fontsize=10, color='darkred')

    # --- 性別子圖 ---
    x_genders = list(gender_ratio.keys())
    y_genders = list(gender_ratio.values())
    sns.barplot(x=x_genders, y=y_genders, ax=axes[2], palette='Greens_r')
    axes[2].set_title('各性別之罹病率\n(該性別糖尿病患 / 該性別總人數)', fontsize=14)
    axes[2].set_xlabel('性別', fontsize=12)
    axes[2].set_ylabel('比率 (Ratio)', fontsize=12)
    for i, v in enumerate(y_genders):
        axes[2].text(i, v + 0.002, f'{v:.3f}', ha='center', fontsize=12, color='darkgreen')

    plt.tight_layout()
    plot_path = 'features_diagnosis_ratio.png'
    plt.savefig(plot_path, dpi=300)
    print(f"\n>>> 三大特徵分佈與比率長條圖皆生成完成！")
    print(f">>> 檔案儲存為: {plot_path}")
    
if __name__ == '__main__':
    main()
