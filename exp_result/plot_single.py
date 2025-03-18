import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.font_manager as fm
import numpy as np

# Try to import scienceplots if available
try:
    import scienceplots
    HAS_SCIENCEPLOTS = True
except ImportError:
    HAS_SCIENCEPLOTS = False
    print("Warning: scienceplots not found. Install with 'pip install SciencePlots' for better styling.")

def setup_formal_style():
    """Setup formal plotting style with professional fonts"""
    # Use science style if available
    if HAS_SCIENCEPLOTS:
        plt.style.use(['science', 'ieee'])
    
    # Disable LaTeX rendering
    plt.rcParams['text.usetex'] = False
    
    # Set font to Times New Roman and configure sizes
    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 14
    plt.rcParams['axes.labelsize'] = 14
    plt.rcParams['axes.titlesize'] = 14
    plt.rcParams['xtick.labelsize'] = 12
    plt.rcParams['ytick.labelsize'] = 12
    plt.rcParams['legend.fontsize'] = 12
    
    # Try to find Times New Roman font
    font_paths = [
        '/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf',  # Linux
        'C:/Windows/Fonts/times.ttf',  # Windows
        '/Library/Fonts/Times New Roman.ttf'  # macOS
    ]
    
    font_prop = None
    for path in font_paths:
        if os.path.exists(path):
            font_prop = fm.FontProperties(fname=path)
            break
    
    # If Times New Roman not found, use a serif font
    if font_prop is None:
        font_prop = fm.FontProperties(family='serif')
    
    return font_prop

def create_throughput_comparison(left_csv, right_csv, sta_index, include_snr=True, output_file=None):
    """Create a comparison plot of left and right throughput for a specific STA"""
    # Setup formal styling
    font_prop = setup_formal_style()
    
    # Read CSV files
    df_left = pd.read_csv(left_csv)
    df_right = pd.read_csv(right_csv)
    
    # Check if data for the requested STA exists
    sta_thr_col = f"STA_{sta_index}_Thr[Mbps]"
    sta_snr_col = f"STA_{sta_index}_MaxSNR[dB]"
    
    if sta_thr_col not in df_left.columns or sta_thr_col not in df_right.columns:
        print(f"Error: STA_{sta_index} data not found in CSV files")
        return
    
    # Create figure and primary axis for throughput
    fig, ax1 = plt.subplots(figsize=(12, 6))
    
    # Colors for professional look
    colors = ['#0C5DA5', '#00B945', '#FF9500', '#FF2C00', '#845B97', '#474747']
    
    # Plot throughput data on primary axis
    ax1.set_xlabel('Time (s)', fontproperties=font_prop)
    ax1.set_ylabel('Throughput (Mbps)', color=colors[0], fontproperties=font_prop)
    
    # Plot left and right throughput
    ax1.plot(df_left['Time [s]'], df_left[sta_thr_col], 
            color=colors[0], linewidth=2,
            label=f'Left AP Throughput')
    ax1.plot(df_right['Time [s]'], df_right[sta_thr_col], 
            color=colors[1], linewidth=2,
            label=f'Right AP Throughput')
    
    # Set up SNR axis if requested
    if include_snr:
        ax2 = ax1.twinx()
        ax2.set_ylabel('SNR (dB)', color=colors[2], fontproperties=font_prop)
        
        # Plot SNR data on secondary axis
        ax2.plot(df_left['Time [s]'], df_left[sta_snr_col], 
                color=colors[2], linestyle='--', linewidth=1.5,
                label=f'Left AP SNR')
        ax2.plot(df_right['Time [s]'], df_right[sta_snr_col], 
                color=colors[3], linestyle='--', linewidth=1.5,
                label=f'Right AP SNR')
        
        # Set reasonable SNR axis range
        snr_min = min(df_left[sta_snr_col].min(), df_right[sta_snr_col].min())
        snr_max = max(df_left[sta_snr_col].max(), df_right[sta_snr_col].max())
        padding = (snr_max - snr_min) * 0.1
        ax2.set_ylim(snr_min - padding, snr_max + padding)
        
        # Apply font to secondary axis ticks
        for label in ax2.get_yticklabels():
            label.set_fontproperties(font_prop)
    
    # Combine legends from both axes
    handles1, labels1 = ax1.get_legend_handles_labels()
    if include_snr:
        handles2, labels2 = ax2.get_legend_handles_labels()
        fig.legend(handles1 + handles2, labels1 + labels2, 
                  loc='upper center', 
                  bbox_to_anchor=(0.5, 0.98),
                  ncol=4,
                  prop=font_prop,
                  frameon=True,
                  edgecolor='black',
                  fancybox=False)
    else:
        fig.legend(handles1, labels1, 
                  loc='upper center', 
                  bbox_to_anchor=(0.5, 0.98),
                  ncol=2,
                  prop=font_prop,
                  frameon=True,
                  edgecolor='black',
                  fancybox=False)
    
    # Apply font to primary axis ticks
    for label in ax1.get_xticklabels() + ax1.get_yticklabels():
        label.set_fontproperties(font_prop)
    
    # Set tick size
    ax1.tick_params(axis='both', which='major', labelsize=12)
    if include_snr:
        ax2.tick_params(axis='both', which='major', labelsize=12)
    
    # Add grid for better readability
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Add title
    # plt.title(f'STA {sta_index} Throughput Comparison Between Left and Right AP', 
    #           fontproperties=font_prop)
    
    # Adjust layout
    plt.subplots_adjust(top=0.9)
    
    # Save or display plot
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Plot saved as {output_file}")
    else:
        plt.tight_layout()
        plt.show()
    
    # Clean up
    plt.close()

def main():
    # Setup command line arguments
    parser = argparse.ArgumentParser(description='Compare left and right throughput for a specific STA')
    parser.add_argument('--left', default='exp_result/left_normalized_thr.csv',
                       help='Path to the left AP throughput CSV file')
    parser.add_argument('--right', default='exp_result/right_normalized_thr.csv',
                       help='Path to the right AP throughput CSV file')
    parser.add_argument('--sta', type=int,
                       help='STA index to plot')
    parser.add_argument('--no-snr', action='store_true',
                       help='Exclude SNR data from the plot')
    parser.add_argument('--output', 
                       help='Output file name (optional)')
    
    args = parser.parse_args()
    
    # Check if files exist
    if not os.path.exists(args.left):
        print(f"Error: Left AP file {args.left} not found")
        return
    if not os.path.exists(args.right):
        print(f"Error: Right AP file {args.right} not found")
        return
    
    # Create plot
    create_throughput_comparison(args.left, args.right, args.sta, 
                                not args.no_snr, args.output)

if __name__ == "__main__":
    main()