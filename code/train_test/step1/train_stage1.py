import numpy as np
import pandas as pd
import pickle

class Stage1RiskCalculator:
    """
    第一階段預測模型 (Lookup Table版)
    透過「年齡」與「性別」查表取得基礎糖尿病盛行率 (基本風險)，
    再透過「BMI」與「性別」查表取得風險乘數 (Hazard Ratio)，
    最終計算: 危險值(%) = 基礎盛行率(%) * BMI風險乘數。
    """
    
    def __init__(self):
        # 1. 基礎盛行率表 (%)
        # 結構: (min_age_inclusive, max_age_inclusive): prevalence
        self.age_prevalence_male = {
            (0, 12): 0.0,
            (13, 15): 0.3,
            (16, 18): 1.1,
            (19, 44): 4.0,
            (45, 64): 15.6,
            (65, 74): 23.9,
            (75, 150): 27.8
        }
        
        self.age_prevalence_female = {
            (0, 12): 0.0,
            (13, 15): 0.2,
            (16, 18): 0.4,
            (19, 44): 1.6,
            (45, 64): 9.9,
            (65, 74): 23.1,
            (75, 150): 31.4
        }
        
        # 2. BMI 風險乘數 (HR 倍率)
        # 範圍: 過輕(<18.5)、正常(18.5-24)、過重(24-28)、肥胖(>28)
        # 注意: 由於浮點數判斷與邊界，我們定義 [18.5, 24], (24, 28] 之類的區間。
        # 為了嚴謹我們以 < 18.5 為過輕，18.5~24包含雙邊為正常，>24到<=28為過重。
        self.bmi_hr_male = {
            'underweight': 0.63, # <18.5
            'normal': 1.00,      # 18.5 <= BMI <= 24
            'overweight': 2.72,  # 24 < BMI <= 28
            'obese': 6.27        # >28
        }
        
        self.bmi_hr_female = {
            'underweight': 0.86, # <18.5
            'normal': 1.00,      # 18.5 <= BMI <= 24
            'overweight': 2.19,  # 24 < BMI <= 28
            'obese': 3.78        # >28
        }

    def _get_age_prevalence(self, age, gender):
        table = self.age_prevalence_male if gender == 1 else self.age_prevalence_female
        for (min_age, max_age), prevalence in table.items():
            if min_age <= age <= max_age:
                return prevalence
        return 0.0 # 若年齡不合理則預設 0

    def _get_bmi_category(self, bmi):
        if bmi < 18.5:
            return 'underweight'
        elif 18.5 <= bmi <= 24:
            return 'normal'
        elif 24 < bmi <= 28:
            return 'overweight'
        else:
            return 'obese'

    def _get_bmi_hr(self, bmi, gender):
        category = self._get_bmi_category(bmi)
        table = self.bmi_hr_male if gender == 1 else self.bmi_hr_female
        return table[category]

    def predict_risk_single(self, age, bmi, gender):
        """計算單一筆資料的危險值(百分比)，並限制最高不高於100%"""
        base_prevalence = self._get_age_prevalence(age, gender)
        hr_multiplier = self._get_bmi_hr(bmi, gender)
        
        final_risk = base_prevalence * hr_multiplier
        return min(final_risk, 100.0)
        
    def predict_proba(self, X):
        """
        相容於原有 ML 模型的介面。
        輸入: Pandas DataFrame，包含欄位 'Age', 'BMI', 'Gender' (其中 Gender: 1=Male, 0=Female)
        輸出: numpy array of shape (n_samples, 2)，其中第一欄為健康機率，第二欄為患病機率
        """
        if not isinstance(X, pd.DataFrame):
            try:
                # 嘗試建立 DataFrame 若傳入的是 Numpy Array 或 List 之類的（以防萬一）
                # 假設外部調用的順序預期為 [Age, BMI, Gender]
                X = pd.DataFrame(X, columns=['Age', 'BMI', 'Gender'])
            except Exception:
                raise ValueError("Input must be a Pandas DataFrame with ['Age', 'BMI', 'Gender']")
            
        probas = []
        for _, row in X.iterrows():
            risk_percent = self.predict_risk_single(row['Age'], row['BMI'], row['Gender'])
            prob1 = float(risk_percent) / 100.0
            prob0 = 1.0 - prob1
            probas.append([prob0, prob1])
            
        return np.array(probas)
        
    def save_model(self, filepath):
        """儲存模型物件到 Pickle"""
        import os
        # 確保目標資料夾存在
        os.makedirs(os.path.dirname(os.path.abspath(filepath)) or '.', exist_ok=True)
        with open(filepath, 'wb') as f:
            pickle.dump(self, f)
        print(f"型號已被轉存至 {filepath} 以供後續階段載入使用。")

if __name__ == '__main__':
    # 建立計算機實例
    model = Stage1RiskCalculator()
    
    # 儲存供之後第二階段使用 (取代原本的 ML model 檔案)
    import os
    save_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stage1_calculator.pkl")
    model.save_model(save_path)
    print(f">>> 系統就緒：第一階段查表預測模型已經更新並輸出至 {save_path}")
