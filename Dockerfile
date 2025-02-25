FROM ubuntu:18.04

# 安裝必要的套件
RUN apt-get update && apt-get install -y \
    vim \
    git \
    cmake \
    build-essential \
    g++-8 \
    openssh-server \
    sudo \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /var/run/sshd \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin no/' /etc/ssh/sshd_config \
    && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config

# 設置 g++-8 為默認編譯器
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 40 && \
    update-alternatives --set g++ /usr/bin/g++-8

# 創建新用戶
ARG USERNAME=admin
ARG USER_PASSWORD=admin

# 使用 adduser 創建用戶並設置密碼
RUN adduser ${USERNAME} \
    && echo "${USERNAME}:${USER_PASSWORD}" | chpasswd \
    && usermod -aG sudo ${USERNAME}

# 切換到新用戶的目錄
WORKDIR /home/${USERNAME}
RUN mkdir -p /home/${USERNAME}/wigig-module \
    && chown ${USERNAME}:${USERNAME} /home/${USERNAME}/wigig-module

# 複製文件到用戶目錄
COPY --chown=${USERNAME}:${USERNAME} . /home/${USERNAME}/wigig-module/

# 創建啟動腳本
RUN echo '#!/bin/bash\n\
service ssh start\n\
tail -f /dev/null\n\
' > /start.sh && chmod +x /start.sh

CMD ["/start.sh"]