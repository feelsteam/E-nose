import pandas as pd
import os
from train_stage1 import Stage1RiskCalculator

def main():
    # 載入模型
    model = Stage1RiskCalculator()

    # 定義測試組距與對應的代表數值
    age_groups = [
        ("7-12歲", 10),
        ("13-15歲", 14),
        ("16-18歲", 17),
        ("19-44歲", 30),
        ("45-64歲", 55),
        ("65-74歲", 70),
        ("75歲以上", 80)
    ]

    bmi_groups = [
        ("過輕 (<18.5)", 17.0),
        ("正常 (18.5-24)", 22.0),
        ("過重 (24-28)", 26.0),
        ("肥胖 (>28)", 30.0)
    ]

    def generate_table_for_gender(gender_val):
        data = []
        for age_label, age_val in age_groups:
            row_dict = {"年齡層": age_label}
            for bmi_label, bmi_val in bmi_groups:
                # 取得危險估計 (百分比)
                risk = model.predict_risk_single(age_val, bmi_val, gender_val)
                row_dict[bmi_label] = f"{risk:.2f}%"
            data.append(row_dict)
            
        return pd.DataFrame(data)

    # 1 代表男性， 0 代表女性
    df_male = generate_table_for_gender(1)
    df_female = generate_table_for_gender(0)

    # 在終端機印出表格
    print("="*60)
    print("                 【男性】各階段綜合危險數值表")
    print("="*60)
    print(df_male.to_string(index=False))
    print("\n\n" + "="*60)
    print("                 【女性】各階段綜合危險數值表")
    print("="*60)
    print(df_female.to_string(index=False))

    # 匯出至 CSV (使用 utf-8-sig 讓 Excel 開啟不亂碼)
    base_dir = os.path.dirname(os.path.abspath(__file__))
    male_csv = os.path.join(base_dir, "risk_table_male.csv")
    female_csv = os.path.join(base_dir, "risk_table_female.csv")
    
    df_male.to_csv(male_csv, index=False, encoding='utf-8-sig')
    df_female.to_csv(female_csv, index=False, encoding='utf-8-sig')
    print(f"\n>>> 表格檔案也已經儲存為 CSV:")
    print(f" - {male_csv}")
    print(f" - {female_csv}")

if __name__ == '__main__':
    main()
