#/!bin/bash

dataRate=100
./waf --run "scratch/dual_testing --fileDir=exp_result/ --ue=8 --vel=0.5 --simulationTime=10 --retrain=true --dataRate=${dataRate}Mbps"

python3 exp_result/normal.py exp_result/left_all_thr.csv --data-rate $dataRate --output exp_result/left_normalized_thr.csv
python3 exp_result/normal.py exp_result/right_all_thr.csv --data-rate $dataRate --output exp_result/right_normalized_thr.csv
python3 exp_result/plot.py exp_result/left_normalized_thr.csv --output exp_result/left_result.png
python3 exp_result/plot.py exp_result/right_normalized_thr.csv --output exp_result/right_result.png


