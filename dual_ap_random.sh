#!/bin/bash

dataRate=100
ue=2
iterations=1  # 设置实验重复次数

# 为每次实验创建一个文件夹
mkdir -p exp_result/multiple_runs

# 循环运行10次实验，每次使用不同的种子
for ((i=1; i<=$iterations; i++))
do
    seed=$i  # 使用循环索引作为种子值
    echo "======================================================"
    echo "Running experiment $i/$iterations with seed $seed"
    echo "======================================================"
    
    # 创建本次实验的输出目录
    run_dir="exp_result/multiple_runs/run_${seed}"
    mkdir -p $run_dir
    
    # 运行NS-3模拟
    ./waf --run "scratch/dual_ap_random --fileDir=exp_result/ --ue=${ue} --vel=0.5 --simulationTime=25 --mac_retx_enable=true --retrain=true --random_walk=false --dataRate=${dataRate}Mbps --seed=${seed} --pcap=true"
    
    # 处理数据
    python3 exp_result/normal.py exp_result/left_all_thr.csv --data-rate $dataRate --output exp_result/left_normalized_thr.csv
    python3 exp_result/normal.py exp_result/right_all_thr.csv --data-rate $dataRate --output exp_result/right_normalized_thr.csv
    
    # 生成图表
    python3 exp_result/plot.py exp_result/left_normalized_thr.csv --output $run_dir/left_result_seed${seed}.png
    python3 exp_result/plot.py exp_result/right_normalized_thr.csv --output $run_dir/right_result_seed${seed}.png
    
    # 轨迹图
    python3 exp_result/position_trace.py --pattern "exp_result/position_STA_*.csv" --batch --output-dir $run_dir/trajectory_plots --show-ap-sectors
    
    # 根据UE数量动态生成plot_single.py命令
    for ((sta=0; sta<$ue; sta++))
    do
        echo "Generating throughput comparison for STA $sta (Seed $seed)"
        python3 exp_result/plot_single.py --sta $sta --output $run_dir/STA${sta}_result_seed${seed}.png
    done
    
    # 将原始数据文件复制到运行目录
    cp exp_result/left_all_thr.csv $run_dir/
    cp exp_result/right_all_thr.csv $run_dir/
    cp exp_result/left_normalized_thr.csv $run_dir/
    cp exp_result/right_normalized_thr.csv $run_dir/
    cp -r exp_result/position_STA_*.csv $run_dir/
    echo "Experiment $i completed. Results saved to $run_dir"
    echo ""
done

echo "All experiments completed!"
