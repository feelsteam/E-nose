import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Set paths
base_dir = r"c:\Users\kfwgx\Project\08_Electronic_Nose\code\train_test"
csv_path = os.path.join(base_dir, "extracted_data.csv")

# Read data
df = pd.read_csv(csv_path)

# Replace 'none' with NaN and convert to float
df.replace('none', np.nan, inplace=True)
df['acetone(空腹狀態)'] = df['acetone(空腹狀態)'].astype(float)
df['acetone(早餐後2h)'] = df['acetone(早餐後2h)'].astype(float)
df['acetone(午餐後2h)'] = df['acetone(午餐後2h)'].astype(float)
df['acetone(晚餐後2h)'] = df['acetone(晚餐後2h)'].astype(float)

plt.rcParams['font.sans-serif'] = ['Microsoft JhengHei', 'SimHei', 'Arial'] # 支援中文字體
plt.rcParams['axes.unicode_minus'] = False

# Define colors
def get_color(type_val):
    if type_val == 'Healthy':
        return 'tab:blue'
    else:
        return 'tab:orange'

from matplotlib.lines import Line2D
legend_elements = [Line2D([0], [0], color='tab:blue', lw=2, marker='o', label='Healthy'),
                   Line2D([0], [0], color='tab:orange', lw=2, marker='o', label='Patient (T1D/T2D)')]

# ---------------------------------------------------------
# 定義一個共用的畫圖 Function (為圖 1, 2, 3 使用)
# ---------------------------------------------------------
def plot_chart(x_labels, y_col_fast, y_col_meal, title, output_filename):
    plt.figure(figsize=(8, 6))
    plt.title(title)
    plt.ylabel("Acetone (ppmv)")
    plt.xticks([0, 1], x_labels)
    plt.xlim(-0.5, 1.5)

    for idx, row in df.iterrows():
        c = get_color(row['type'])
        y_fast = row[y_col_fast]
        y_meal = row[y_col_meal]
        
        # 若同時有兩點資料，則連線
        if pd.notna(y_fast) and pd.notna(y_meal):
            plt.plot([0, 1], [y_fast, y_meal], color=c, marker='o', linestyle='-', alpha=0.7)
        # 若只有單點，單獨畫點
        else:
            if pd.notna(y_fast):
                plt.plot([0], [y_fast], color=c, marker='o', alpha=0.7)
            if pd.notna(y_meal):
                plt.plot([1], [y_meal], color=c, marker='o', alpha=0.7)

    plt.legend(handles=legend_elements, loc='upper right')
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    plt.savefig(os.path.join(base_dir, output_filename), dpi=300)
    plt.close()

# ------------- 繪製圖 1, 2, 3 -------------
plot_chart(["Fasting (空腹)", "Post-Breakfast 2h (早餐飯後)"], 'acetone(空腹狀態)', 'acetone(早餐後2h)', 
           "Acetone: Fasting vs Post-Breakfast", "chart1_breakfast.png")

plot_chart(["Fasting (空腹)", "Post-Lunch 2h (午餐飯後)"], 'acetone(空腹狀態)', 'acetone(午餐後2h)', 
           "Acetone: Fasting vs Post-Lunch", "chart2_lunch.png")

plot_chart(["Fasting (空腹)", "Post-Dinner 2h (晚餐飯後)"], 'acetone(空腹狀態)', 'acetone(晚餐後2h)', 
           "Acetone: Fasting vs Post-Dinner", "chart3_dinner.png")

# ---------------------------------------------------------
# 繪製圖 4: 全部的空腹與餐後比較 (同一個人可能會有多條線)
# ---------------------------------------------------------
plt.figure(figsize=(8, 6))
plt.title("Acetone: Fasting vs Post-Meal (All Meals) (空腹 vs 所有的餐後)")
plt.ylabel("Acetone (ppmv)")
plt.xticks([0, 1], ["Fasting (空腹)", "Post-Meal (餐後)"])
plt.xlim(-0.5, 1.5)

# 同時記錄給圖 5 和圖 6 使用的數據
slopes = {'Healthy': [], 'Diabetic': []}
fasting_vals = {'Healthy': [], 'Diabetic': []}

for idx, row in df.iterrows():
    c = get_color(row['type'])
    patient_group = 'Healthy' if row['type'] == 'Healthy' else 'Diabetic'
    y_fast = row['acetone(空腹狀態)']
    meals = [row['acetone(早餐後2h)'], row['acetone(午餐後2h)'], row['acetone(晚餐後2h)']]
    
    plotted_fast_line = False
    
    for y_meal in meals:
        if pd.notna(y_fast) and pd.notna(y_meal):
            plt.plot([0, 1], [y_fast, y_meal], color=c, marker='o', linestyle='-', alpha=0.5)
            plotted_fast_line = True
            
            # 紀錄斜率 (Y後 - Y前) 與 空腹值
            slope = y_meal - y_fast
            slopes[patient_group].append(slope)
            fasting_vals[patient_group].append(y_fast)
        else:
            # 只有餐後的孤立點
            if pd.notna(y_meal):
                plt.plot([1], [y_meal], color=c, marker='o', alpha=0.5)
                
    # 如果完全沒有餐後資料，只有空腹，那保留一個單點
    if pd.notna(y_fast) and not plotted_fast_line:
        plt.plot([0], [y_fast], color=c, marker='o', alpha=0.5)

plt.legend(handles=legend_elements, loc='upper right')
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.savefig(os.path.join(base_dir, "chart4_all_meals.png"), dpi=300)
plt.close()

# ---------------------------------------------------------
# 繪製圖 5: 斜率比較 (健康 vs 糖尿病 的類別分佈)
# ---------------------------------------------------------
plt.figure(figsize=(8, 6))
plt.title("Slopes of Acetone Change (空腹到餐後的斜率分佈)")
plt.ylabel("Slope (Δ Acetone ppmv)")

healthy_slopes = slopes['Healthy']
diabetic_slopes = slopes['Diabetic']

# 加入水平抖動 (Jitter) 來避免散佈點重疊
def add_jitter(x_val, num_points, width=0.1):
    return np.random.normal(x_val, width, num_points)

if healthy_slopes:
    plt.scatter(add_jitter(0, len(healthy_slopes)), healthy_slopes, color='tab:blue', alpha=0.7)
    plt.hlines(np.mean(healthy_slopes), -0.2, 0.2, color='darkblue', linewidth=2, zorder=3)

if diabetic_slopes:
    plt.scatter(add_jitter(1, len(diabetic_slopes)), diabetic_slopes, color='tab:orange', alpha=0.7)
    plt.hlines(np.mean(diabetic_slopes), 0.8, 1.2, color='darkorange', linewidth=2, zorder=3)

plt.xticks([0, 1], ["Healthy (健康)", "Diabetic (糖尿病)"])
plt.xlim(-0.5, 1.5)
plt.axhline(0, color='gray', linestyle='--', linewidth=1) # 加入斜率為 0 的對比線
plt.grid(axis='y', linestyle='--', alpha=0.7)

legend_slope = [Line2D([0], [0], color='tab:blue', lw=0, marker='o', label='Healthy'),
                Line2D([0], [0], color='tab:orange', lw=0, marker='o', label='Diabetic (T1D/T2D)')]
plt.legend(handles=legend_slope, loc='upper right')
plt.savefig(os.path.join(base_dir, "chart5_slopes.png"), dpi=300)
plt.close()

# ---------------------------------------------------------
# 繪製圖 6: 空腹丙酮值 vs 斜率
# ---------------------------------------------------------
plt.figure(figsize=(8, 6))
plt.title("Fasting Acetone vs Slope (空腹丙酮值 vs 變化斜率)")
plt.xlabel("Fasting Acetone (ppmv)")
plt.ylabel("Slope (Δ Acetone ppmv)")

if healthy_slopes:
    plt.scatter(fasting_vals['Healthy'], healthy_slopes, color='tab:blue', alpha=0.7)

if diabetic_slopes:
    plt.scatter(fasting_vals['Diabetic'], diabetic_slopes, color='tab:orange', alpha=0.7)

plt.axhline(0, color='gray', linestyle='--', linewidth=1) # 加入斜率為 0 的對比線
plt.grid(True, linestyle='--', alpha=0.7)

plt.legend(handles=legend_slope, loc='best')
plt.savefig(os.path.join(base_dir, "chart6_fasting_vs_slope.png"), dpi=300)
plt.close()

print(f"All 6 charts successfully generated in {base_dir}")
