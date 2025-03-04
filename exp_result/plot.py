import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.font_manager as fm
import scienceplots

# 設置科學風格
plt.style.use(['science', 'ieee'])  # 只使用基本的科學風格和 IEEE 風格

# 禁用 LaTeX 渲染 - 確保在設置風格後再禁用
plt.rcParams['text.usetex'] = False
plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['font.size'] = 14

# 添加字體
font_path = '/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf'
font_prop = fm.FontProperties(fname=font_path)

def create_plot(csv_file, patterns, output_file=None):
    # 讀取 CSV 檔案
    df = pd.read_csv(csv_file)
    
    # 創建圖表
    fig, ax1 = plt.subplots(figsize=(12, 6))
    
    # 設置第一個 y 軸 (吞吐量)
    color1 = 'tab:blue'
    ax1.set_xlabel('Time (s)', fontproperties=font_prop)
    ax1.set_ylabel('Normalized Throughput', color=color1, fontproperties=font_prop)
    
    # 設置第二個 y 軸 (SNR)
    ax2 = ax1.twinx()
    color2 = 'tab:red'
    ax2.set_ylabel('SNR (dB)', color=color2, fontproperties=font_prop)
    ax2.set_ylim(-5, 25)  # 設置 SNR 軸範圍
    
    # 顏色列表 - 使用更專業的配色
    colors = ['#0C5DA5', '#00B945', '#FF9500', '#FF2C00', '#845B97', '#474747']
    
    # 定義標籤映射
    label_map = {
        'STA_0_Thr': 'STA0 Throughput',
        'Non_Moving_Thr': 'Non-Moving Throughput',
        'STA_0_MaxSNR': 'STA0 SNR',
        'STA_1_MaxSNR': 'Non-Moving SNR'
    }
    
    # 繪製每個模式的數據
    for i, pattern in enumerate(patterns):
        base_label = pattern.split('[')[0]
        display_label = label_map.get(base_label, base_label)
        
        if 'SNR' in pattern:
            ax2.plot(df['Time [s]'], df[pattern], 
                    color=colors[i+2], 
                    label=display_label)
        else:
            ax1.plot(df['Time [s]'], df[pattern], 
                    color=colors[i], 
                    label=display_label)
    
    # 合併兩個軸的圖例
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    
    # 使用更好的圖例樣式
    fig.legend(lines1 + lines2, labels1 + labels2, 
              loc='upper center', 
              bbox_to_anchor=(0.51,0.98),
              ncol=4,
              fontsize=10,
              prop=font_prop,
              frameon=True,
              edgecolor='black',
              fancybox=False)
    
    # 調整布局以適應頂部的圖例
    plt.subplots_adjust(top=0.9)
    
    # 設置刻度字體
    for label in ax1.get_xticklabels() + ax1.get_yticklabels():
        label.set_fontproperties(font_prop)
    
    for label in ax2.get_yticklabels():
        label.set_fontproperties(font_prop)
    
    # 設置刻度大小
    ax1.tick_params(axis='both', which='major', labelsize=10)
    ax2.tick_params(axis='both', which='major', labelsize=10)
    
    # 儲存或顯示圖表
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Plot saved as {output_file}")
    else:
        plt.show()
    
    # 清除圖表
    plt.close()

def main():
    # 設置命令列參數
    parser = argparse.ArgumentParser(description='Plot throughput and SNR data from CSV file')
    parser.add_argument('csv_file', help='Path to the CSV file')
    parser.add_argument('--patterns', nargs='+', 
                      default=['STA_0_Thr[Mbps]', 'Non_Moving_Thr[Mbps]', 
                              'STA_0_MaxSNR[dB]', 'STA_1_MaxSNR[dB]'],
                      help='Data patterns to plot')
    parser.add_argument('--output', help='Output file name (optional)')
    
    args = parser.parse_args()
    
    # 檢查檔案是否存在
    if not os.path.exists(args.csv_file):
        print(f"Error: File {args.csv_file} not found")
        return
    
    # 創建圖表
    create_plot(args.csv_file, args.patterns, args.output)

if __name__ == "__main__":
    main()
