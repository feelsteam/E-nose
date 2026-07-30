'''
這是一個用來產生 Acetone (丙酮) 濃度隨時間變化趨勢圖的程式。
主要功能：
1. 繪製健康受試者 (Healthy) 與糖尿病患者 (Diabetic) 的對比曲線。
2. 使用分段三次樣條插值 (Cubic Spline) 確保「進食後」為平滑峰值 (斜率為 0)。
3. 標註各時間點的具體數值。
4. 繪製空腹點的水平基準線 (Baseline)。
5. 自動儲存高品質 (300 DPI) 的 PNG 趨勢圖檔案。
'''

import matplotlib.pyplot as plt
import numpy as np
from scipy.interpolate import make_interp_spline
import os

# 設定支援中文顯示的字體 (Windows 預設微軟正黑體)
plt.rcParams['font.sans-serif'] = ['Microsoft JhengHei', 'SimHei', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False

# X軸標籤與對應的數值 (為了平滑取線需要轉成數值)
x_labels = ['Fasting(空腹)', '進食後', '飯後1h', '飯後2h', '飯後3h']
x = np.arange(len(x_labels))

# Y軸資料點 (Acetone ppm)
y_healthy = np.array([1.1, 2.4, 1.6, 1.25, 1.19])
y_diabetic = np.array([2.0, 3.4, 3.3, 3.1, 3.0])

# 產生平滑曲線的函式：確保進食後 (index 1) 斜率為 0 (最高點)
from scipy.interpolate import CubicSpline

def get_peak_smooth_curve(x_pts, y_pts, start_zero=False, end_zero=False):
    # 分成兩段插值：0->1 和 1->4
    # 第一段：從空腹到進食後，終點 (index 1) 斜率設為 0
    bc_start = (1, 0.0) if start_zero else 'not-a-knot'
    cs1 = CubicSpline(x_pts[:2], y_pts[:2], bc_type=(bc_start, (1, 0.0)))
    
    # 第二段：從進食後到末端，起點 (index 1) 斜率設為 0
    bc_end = (1, 0.0) if end_zero else 'not-a-knot'
    cs2 = CubicSpline(x_pts[1:], y_pts[1:], bc_type=((1, 0.0), bc_end))
    
    # 產生細分點
    xs1 = np.linspace(x_pts[0], x_pts[1], 100)
    xs2 = np.linspace(x_pts[1], x_pts[-1], 300)
    
    # 合併結果 (去重疊點)
    x_final = np.concatenate([xs1, xs2[1:]])
    y_final = np.concatenate([cs1(xs1), cs2(xs2)[1:]])
    return x_final, y_final

# 產生平滑曲線
# 健康組：起點(空腹)、最高點(進食後)、終點(3h) 斜率均設為 0
x_smooth, y_healthy_smooth = get_peak_smooth_curve(x, y_healthy, start_zero=True, end_zero=True)

# 糖尿病組：最高點(進食後) 斜率設為 0
_, y_diabetic_smooth = get_peak_smooth_curve(x, y_diabetic, start_zero=False, end_zero=False)

# 開始繪圖
plt.figure(figsize=(10, 6))

# 繪製平滑曲線
plt.plot(x_smooth, y_healthy_smooth, color='blue', label='健康 (Healthy)', linewidth=2.5)
plt.plot(x_smooth, y_diabetic_smooth, color='darkorange', label='糖尿病 (Diabetic)', linewidth=2.5)

# 繪製實際的資料點
plt.scatter(x, y_healthy, color='blue', s=60, zorder=5)
plt.scatter(x, y_diabetic, color='darkorange', s=60, zorder=5)

# --- 新增空腹基準值水平線 ---
# 健康組基準值
plt.axhline(y=y_healthy[0], color='blue', linestyle='--', alpha=0.7, linewidth=1.5)
plt.text(x[-1], y_healthy[0] - 0.05, '基準值 (Baseline)', color='blue', alpha=0.8, fontsize=10, ha='right', va='top', fontweight='bold')

# 糖尿病組基準值
plt.axhline(y=y_diabetic[0], color='darkorange', linestyle='--', alpha=0.7, linewidth=1.5)
plt.text(x[-1], y_diabetic[0] - 0.05, '基準值 (Baseline)', color='darkorange', alpha=0.8, fontsize=10, ha='right', va='top', fontweight='bold')
# -------------------------

# 在各點旁邊標示具體數值 (選用，讓圖表更清晰)
for i, txt in enumerate(y_healthy):
    plt.annotate(f'{txt}', (x[i], y_healthy[i]), textcoords="offset points", xytext=(0, 10), ha='center', color='blue', fontweight='bold')
for i, txt in enumerate(y_diabetic):
    plt.annotate(f'{txt}', (x[i], y_diabetic[i]), textcoords="offset points", xytext=(0, 10), ha='center', color='darkorange', fontweight='bold')

# 設定XY軸標籤與範圍
plt.xticks(x, x_labels, fontsize=12)
plt.yticks(fontsize=11)
plt.ylim(0.5, 3.8)
plt.ylabel('Acetone (ppm)', fontsize=13)
plt.xlabel('Time (時間)', fontsize=13)
plt.title('Acetone Level Over Time', fontsize=16, fontweight='bold')

# 加上圖例與網格
plt.legend(fontsize=12)
plt.grid(True, linestyle='--', alpha=0.6)

# 儲存圖表與顯示
output_path = os.path.join(os.path.dirname(__file__), 'acetone_trend_plot.png')
plt.tight_layout()
plt.savefig(output_path, dpi=300)

print(f"圖表已成功生成並儲存於: {output_path}")
plt.show() # 如果在有 GUI 的環境可以直接跳出視窗觀看
