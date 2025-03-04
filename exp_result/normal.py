# coding: utf-8
import pandas as pd
import numpy as np
import argparse
import os

def normalize_throughput(input_path, data_rate=None, output_path=None):
    # 檢查輸入檔案是否存在
    if not os.path.exists(input_path):
        raise FileNotFoundError(f"找不到輸入檔案: {input_path}")
    
    # 讀取 CSV 檔案
    df = pd.read_csv(input_path)
    
    # 找出所有吞吐量的欄位 (排除包含 SNR 的欄位)
    throughput_cols = [col for col in df.columns if 'Thr' in col]
    num_single_throughput = len(throughput_cols) - 2
    print(f"num_single_throughput: {num_single_throughput}")
    # 正規化吞吐量
    df_normalized = df.copy()
    if data_rate is not None:
        # 使用指定的 data rate 進行正規化
        df_normalized[throughput_cols[0:num_single_throughput]] = df[throughput_cols[0:num_single_throughput]] / data_rate
        df_normalized[throughput_cols[num_single_throughput:]] = df[throughput_cols[num_single_throughput:]] / (data_rate*(num_single_throughput))
    else:
        # 使用每個欄位的最大值進行正規化
        max_values = df[throughput_cols].max()
        df_normalized[throughput_cols] = df[throughput_cols] / max_values
    
    # 如果沒有指定輸出路徑，則在輸入檔案的相同目錄下生成輸出檔案
    if output_path is None:
        output_dir = os.path.dirname(input_path)
        input_filename = os.path.basename(input_path)
        output_filename = 'normalized_' + input_filename
        output_path = os.path.join(output_dir, output_filename)
    
    # 確保輸出目錄存在
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    # 儲存正規化後的結果
    df_normalized.to_csv(output_path, index=False)
    print(f"正規化完成，結果已儲存至: {output_path}")

if __name__ == "__main__":
    # 設定命令列參數
    parser = argparse.ArgumentParser(description='對CSV檔案中的吞吐量數據進行正規化')
    parser.add_argument('input_path', help='輸入CSV檔案的路徑')
    parser.add_argument('-r', '--data-rate', type=float, help='指定的 data rate (Mbps)，用於正規化', default=None)
    parser.add_argument('-o', '--output', help='輸出CSV檔案的路徑 (可選)', default=None)
    
    # 解析命令列參數
    args = parser.parse_args()
    
    # 執行正規化
    normalize_throughput(args.input_path, args.data_rate, args.output)
