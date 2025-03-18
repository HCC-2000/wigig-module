import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse
import glob
import os
import matplotlib.patches as patches
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

def draw_ap_sectors(ax, show_ap_labels=True):
    """Draw AP positions and sectors on the plot"""
    # AP positions
    ap_left_pos = (-1, 1)
    ap_right_pos = (1, 1)
    
    # Draw AP points
    ax.plot(ap_left_pos[0], ap_left_pos[1], 's', color='skyblue', 
            markersize=10, markeredgecolor='blue', zorder=10)
    ax.plot(ap_right_pos[0], ap_right_pos[1], 's', color='skyblue', 
            markersize=10, markeredgecolor='blue', zorder=10)
    
    # Add AP labels
    if show_ap_labels:
        ax.annotate('AP Left\n(-1, 0, 5)', ap_left_pos, 
                   xytext=(0, 10), textcoords='offset points',
                   ha='center', fontsize=9, bbox=dict(boxstyle="round,pad=0.3", 
                                                     fc='skyblue', ec='blue', alpha=0.7))
        ax.annotate('AP Right\n(1, 0, 5)', ap_right_pos, 
                   xytext=(0, 10), textcoords='offset points',
                   ha='center', fontsize=9, bbox=dict(boxstyle="round,pad=0.3", 
                                                     fc='skyblue', ec='blue', alpha=0.7))
    
    # Draw 8 sectors (for each AP)
    radius = 2.0  # sector radius
    sector_colors = ['lightblue', 'azure', 'lightcyan', 'lavender']
    
    # Left AP sectors
    for i in range(8):
        angle_start = i * 45 - 22.5  # 从-22.5度开始
        angle_end = (i + 1) * 45 - 22.5
        color = sector_colors[i % len(sector_colors)]
        wedge = patches.Wedge(ap_left_pos, radius, angle_start, angle_end, 
                             alpha=0.2, color=color, ec='blue', lw=0.5)
        ax.add_patch(wedge)
        # Add sector number (从1开始而不是0)
        mid_angle = np.radians((angle_start + angle_end) / 2)
        x = ap_left_pos[0] + 0.7 * radius * np.cos(mid_angle)
        y = ap_left_pos[1] + 0.7 * radius * np.sin(mid_angle)
        ax.text(x, y, str(i+1), ha='center', va='center', fontsize=8)
    
    # Right AP sectors
    for i in range(8):
        angle_start = i * 45 - 22.5  # 从-22.5度开始
        angle_end = (i + 1) * 45 - 22.5
        color = sector_colors[i % len(sector_colors)]
        wedge = patches.Wedge(ap_right_pos, radius, angle_start, angle_end, 
                             alpha=0.2, color=color, ec='blue', lw=0.5)
        ax.add_patch(wedge)
        # Add sector number (从1开始而不是0)
        mid_angle = np.radians((angle_start + angle_end) / 2)
        x = ap_right_pos[0] + 0.7 * radius * np.cos(mid_angle)
        y = ap_right_pos[1] + 0.7 * radius * np.sin(mid_angle)
        ax.text(x, y, str(i+1), ha='center', va='center', fontsize=8)

def plot_single_trajectory(csv_file, output_file=None, show_time_labels=True, time_step=5, show_ap_sectors=False):
    """Plot trajectory for a single file"""
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Read CSV file
    df = pd.read_csv(csv_file)
    
    # Get node ID from filename
    filename = os.path.basename(csv_file)
    node_parts = filename.split('_')
    node_id = "unknown"
    if len(node_parts) > 1:
        node_id = node_parts[1].split('.')[0]
    
    # Plot main trajectory
    ax.plot(df['X [m]'], df['Y [m]'], '-', color='blue', 
             linewidth=1.5, label=f'Node {node_id}')
    
    # Use points to show time changes
    scatter = ax.scatter(df['X [m]'], df['Y [m]'], c=df['Time [s]'], 
                cmap='viridis', s=30, alpha=0.7)
    
    # Mark start and end points
    ax.plot(df['X [m]'].iloc[0], df['Y [m]'].iloc[0], 'go', 
            markersize=10, label='Start')
    ax.plot(df['X [m]'].iloc[-1], df['Y [m]'].iloc[-1], 'ro', 
            markersize=10, label='End')
    
    # Add time labels
    if show_time_labels:
        time_indices = np.arange(0, len(df), 
                                max(1, int(len(df) * time_step / df['Time [s]'].max())))
        for idx in time_indices:
            ax.annotate(f"{df['Time [s]'].iloc[idx]:.1f}s", 
                       (df['X [m]'].iloc[idx], df['Y [m]'].iloc[idx]),
                       xytext=(5, 5), textcoords='offset points',
                       fontsize=8)
    
    # Add AP sectors
    if show_ap_sectors:
        draw_ap_sectors(ax)
    
    # Add colorbar for time
    cbar = plt.colorbar(scatter)
    cbar.set_label('Time (s)')
    
    # Add grid lines and set equal aspect ratio
    ax.grid(True, linestyle='--', alpha=0.7)
    ax.set_aspect('equal')
    
    # Set axis limits (to ensure AP sectors are visible)
    if show_ap_sectors:
        ax.set_xlim(-3.5, 3.5)
        ax.set_ylim(-3.5, 3.5)
    
    # Add title and axis labels
    plt.title(f'Movement Trajectory - Node {node_id}')
    plt.xlabel('X Position (m)')
    plt.ylabel('Y Position (m)')
    plt.legend()
    
    # Save or display
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Trajectory plot saved as: {output_file}")
    
    plt.close()  # Close the figure to free memory

def plot_multiple_trajectories(csv_files, output_file=None, show_time_labels=True, time_step=5, show_ap_sectors=False):
    """Plot multiple trajectories on a single figure"""
    fig, ax = plt.subplots(figsize=(12, 10))
    
    # Use different colors for different nodes
    colors = plt.cm.tab10.colors
    
    for i, csv_file in enumerate(csv_files):
        # Read CSV file
        df = pd.read_csv(csv_file)
        
        # Get node ID from filename
        filename = os.path.basename(csv_file)
        node_parts = filename.split('_')
        node_id = "unknown"
        if len(node_parts) > 1:
            node_id = node_parts[1].split('.')[0]
        
        color = colors[i % len(colors)]
        
        # Plot main trajectory
        ax.plot(df['X [m]'], df['Y [m]'], '-', color=color, 
                 linewidth=1.5, label=f'Node {node_id}')
        
        # Use points to show time changes
        ax.scatter(df['X [m]'], df['Y [m]'], c=df['Time [s]'], 
                    cmap='viridis', s=30, alpha=0.7)
        
        # Mark start and end points
        ax.plot(df['X [m]'].iloc[0], df['Y [m]'].iloc[0], 'o', color=color, 
                markersize=10, markeredgecolor='black')
        ax.plot(df['X [m]'].iloc[-1], df['Y [m]'].iloc[-1], '*', color=color, 
                markersize=12, markeredgecolor='black')
        
        # Add time labels
        if show_time_labels:
            time_indices = np.arange(0, len(df), 
                                    max(1, int(len(df) * time_step / df['Time [s]'].max())))
            for idx in time_indices:
                ax.annotate(f"{df['Time [s]'].iloc[idx]:.1f}s", 
                           (df['X [m]'].iloc[idx], df['Y [m]'].iloc[idx]),
                           xytext=(5, 5), textcoords='offset points',
                           fontsize=8)
    
    # Add AP sectors
    if show_ap_sectors:
        draw_ap_sectors(ax)
        # Set axis limits to ensure AP sectors are visible
        ax.set_xlim(-3.5, 3.5)
        ax.set_ylim(-3.5, 3.5)
    
    # Add grid lines and coordinate range
    ax.grid(True, linestyle='--', alpha=0.7)
    ax.set_aspect('equal')
    
    # Add title and axis labels
    plt.title('Node Movement Trajectories')
    plt.xlabel('X Position (m)')
    plt.ylabel('Y Position (m)')
    plt.legend()
    
    # Save or display
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Combined trajectory plot saved as: {output_file}")
    
    plt.close()  # Close the figure to free memory

def batch_process(csv_files, output_dir, show_time_labels=True, time_step=5, show_ap_sectors=False):
    """Process each file separately and generate individual plots"""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created output directory: {output_dir}")
    
    print(f"Processing {len(csv_files)} files in batch mode...")
    
    for csv_file in csv_files:
        # Generate output filename based on input filename
        filename = os.path.basename(csv_file)
        output_file = os.path.join(output_dir, f"{os.path.splitext(filename)[0]}_trajectory.png")
        
        # Plot and save individual trajectory
        plot_single_trajectory(csv_file, output_file, show_time_labels, time_step, show_ap_sectors)
    
    print(f"Batch processing completed. {len(csv_files)} files processed.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot node movement trajectory')
    parser.add_argument('--files', nargs='+', help='CSV file paths (multiple allowed)')
    parser.add_argument('--pattern', help='CSV file matching pattern, e.g., "position_STA_*.csv"')
    parser.add_argument('--output', help='Output image filename (for combined plot)')
    parser.add_argument('--output-dir', default='trajectory_plots', 
                       help='Output directory for batch processing')
    parser.add_argument('--time-step', type=float, default=5, 
                       help='Time label interval (seconds)')
    parser.add_argument('--no-time-labels', action='store_true', 
                       help='Do not show time labels')
    parser.add_argument('--batch', action='store_true',
                       help='Process each file separately and generate individual plots')
    parser.add_argument('--combined', action='store_true',
                       help='Also generate a combined plot in batch mode')
    parser.add_argument('--show-ap-sectors', action='store_true',
                       help='Show AP positions and sectors')
    
    args = parser.parse_args()
    
    # Find files to process
    if args.pattern:
        files = glob.glob(args.pattern)
    elif args.files:
        files = args.files
    else:
        files = ["position_STA_0.csv"]  # Default file
    
    if len(files) == 0:
        print("No files found matching the pattern. Please check your input.")
        exit(1)
    
    print(f"Found {len(files)} files:")
    for file in files:
        print(f"  - {file}")
    
    # Process files based on selected mode
    if args.batch:
        # Batch process mode - generate individual plots
        batch_process(files, args.output_dir, not args.no_time_labels, args.time_step, args.show_ap_sectors)
        
        # Also generate combined plot if requested
        if args.combined:
            combined_output = args.output if args.output else os.path.join(args.output_dir, "combined_trajectory.png")
            plot_multiple_trajectories(files, combined_output, not args.no_time_labels, args.time_step, args.show_ap_sectors)
    else:
        # Standard mode - generate a single combined plot
        output_file = args.output if args.output else "combined_trajectory.png"
        plot_multiple_trajectories(files, output_file, not args.no_time_labels, args.time_step, args.show_ap_sectors)