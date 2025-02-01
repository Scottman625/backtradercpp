import sys
import os
import numpy as np
import pandas as pd
from datetime import datetime, timedelta

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../build/Release")))
import backtradercpp


# 產生假資料用的輔助函數
def generate_fake_stock_data(stock_code, stock_name, start_date, end_date, industry_type):
    # 確保生成至少 500 個交易日的數據
    dates = pd.date_range(start=start_date, end=end_date, freq='B')
    if len(dates) < 500:
        end_date = pd.to_datetime(start_date) + pd.Timedelta(days=800)
        dates = pd.date_range(start=start_date, end=end_date, freq='B')
    
    n = len(dates)
    print(f"Generating {n} trading days for {stock_name} ({stock_code})")
    
    # 使用更隨機的種子生成方式
    seed = np.random.randint(0, 1000000)
    np.random.seed(seed)
    
    # 根據產業類型設定不同的基本參數
    industry_params = {
        'semiconductor': {  # 半導體
            'base_price_range': (300, 800),
            'volatility_range': (0.012, 0.02),
            'volume_base': (1000000, 5000000)
        },
        'financial': {  # 金融
            'base_price_range': (20, 100),
            'volatility_range': (0.008, 0.015),
            'volume_base': (2000000, 8000000)
        },
        'electronics': {  # 電子
            'base_price_range': (100, 500),
            'volatility_range': (0.01, 0.018),
            'volume_base': (800000, 3000000)
        },
        'traditional': {  # 傳統產業
            'base_price_range': (30, 200),
            'volatility_range': (0.006, 0.012),
            'volume_base': (500000, 2000000)
        }
    }
    
    # 獲取產業參數
    params = industry_params[industry_type]
    
    # 生成基礎價格
    base_price = np.random.uniform(*params['base_price_range'])
    
    # 添加季節性和趨勢
    t = np.linspace(0, 3, n)  # 時間序列
    trend = np.random.uniform(-0.2, 0.3) * t  # 隨機趨勢
    seasonal = 0.05 * np.sin(2 * np.pi * t)  # 季節性波動
    
    # 生成價格序列
    volatility = np.random.uniform(*params['volatility_range'])
    daily_returns = np.random.normal(0.0002, volatility, n)
    noise = np.random.normal(0, 0.001, n)  # 添加小噪音
    
    # 組合所有因素
    price_multipliers = np.exp(np.cumsum(daily_returns) + trend + seasonal + noise)
    prices = base_price * price_multipliers
    
    # 生成成交量
    base_volume = np.random.randint(*params['volume_base'])
    volume_volatility = np.random.uniform(0.3, 0.6)
    volumes = np.random.lognormal(np.log(base_volume), volume_volatility, n).astype(int)
    
    # 生成價格數據
    df = pd.DataFrame({
        'stock_code': stock_code,
        'stock_name': stock_name,
        'date': dates,
        'Open': prices * (1 + np.random.normal(0, 0.002, n)),
        'High': prices * (1 + np.random.uniform(0.001, 0.008, n)),
        'Low': prices * (1 - np.random.uniform(0.001, 0.008, n)),
        'Close': prices * (1 + np.random.normal(0, 0.002, n)),
        'Volume': volumes
    })
    
    # 重置隨機種子
    np.random.seed(None)
    
    return df

def run_backtrader_in_process(serialized_data_chunk):
    data_list = []

    for serialized_data in serialized_data_chunk:
        all_arrays = []
        
        for stock_data in serialized_data['stocks']:
            stock_array = stock_data['array']
            all_arrays.extend(stock_array)

        numpy_array = np.array(all_arrays)
        df = pd.DataFrame(numpy_array, columns=serialized_data['columns'])

        assets_ = len(df['stock_code'].unique())
        
        # 創建 C++ 的 DataFrame 對象
        df_cpp = backtradercpp.DataFrame()
        df_cpp.data = df.astype(str).values.tolist()
        df_cpp.columns = df.columns.tolist()
        data_list.append(df_cpp)

    print("Start Running C++ Backtrader in Process...")
    backtradercpp.runAll(data_list, assets_)

def runBacktrader():
    # 生成三年的測試資料
    start_date = '2021-01-01'
    end_date = '2023-12-31'
    
    # 25檔測試股票，加入產業類型
    stocks = [
        ('2330', '台積電', 'semiconductor'), ('2317', '鴻海', 'electronics'),
        ('2454', '聯發科', 'semiconductor'), ('2412', '中華電', 'traditional'),
        ('2308', '台達電', 'electronics'), ('2303', '聯電', 'semiconductor'),
        ('2881', '富邦金', 'financial'), ('2882', '國泰金', 'financial'),
        ('1301', '台塑', 'traditional'), ('2002', '中鋼', 'traditional'),
        ('3711', '日月光投控', 'semiconductor'), ('2891', '中信金', 'financial'),
        ('2886', '兆豐金', 'financial'), ('1303', '南亞', 'traditional'),
        ('2884', '玉山金', 'financial'), ('2885', '元大金', 'financial'),
        ('2892', '第一金', 'financial'), ('2880', '華南金', 'financial'),
        ('2887', '台新金', 'financial'), ('2890', '永豐金', 'financial'),
        ('5871', '中租-KY', 'financial'), ('2379', '瑞昱', 'semiconductor'),
        ('3045', '台灣大', 'traditional'), ('2912', '統一超', 'traditional'),
        ('2357', '華碩', 'electronics')
    ]
    
    print(f"開始生成 {len(stocks)} 檔股票的測試數據...")
    print(f"測試期間: {start_date} 到 {end_date}")
    
    # 合併所有股票資料
    dfs = []
    for code, name, industry in stocks:
        df = generate_fake_stock_data(code, name, start_date, end_date, industry)
        dfs.append(df)
    
    df = pd.concat(dfs, ignore_index=True)
    df['date'] = pd.to_datetime(df['date'])
    
    print("\n數據統計:")
    print(f"總資料筆數: {len(df)}")
    print(f"每檔股票平均資料筆數: {len(df) // len(stocks)}")
    print(f"日期範圍: {df['date'].min()} 到 {df['date'].max()}")
    
    print("\nStart grouping data by date and stock_code...")
    
    # 根據日期和股票代碼分組
    grouped = df.groupby(['date', 'stock_code'])
    
    # 將每個分組轉換為可傳遞的數據塊
    data_by_date = {}
    for (date, stock_code), group in grouped:
        serialized_data = {
            'array': group.values.tolist(),
        }
        if date not in data_by_date:
            data_by_date[date] = []
        data_by_date[date].append(serialized_data)
        columns = group.columns.tolist()
    
    # 按日期處理，確保每個日期下的多檔股票可以同時處理
    data_list = []
    for date, stock_data in data_by_date.items():
        data_list.append({
            'date': date,
            'stocks': stock_data,
            'columns': columns
        })
    
    print("\nStart Running C++ Backtrader...")
    print(f"處理 {len(data_list)} 個交易日的數據")
    run_backtrader_in_process(data_list)

if __name__ == "__main__":
    runBacktrader()