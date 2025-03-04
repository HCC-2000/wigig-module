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
    python3-pip \
    ttf-mscorefonts-installer \
    # pyenv 依賴包
    make \
    build-essential \
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    libreadline-dev \
    libsqlite3-dev \
    wget \
    curl \
    llvm \
    libncurses5-dev \
    libncursesw5-dev \
    xz-utils \
    tk-dev \
    libffi-dev \
    liblzma-dev \
    python-openssl \
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

# 切換到新用戶
USER ${USERNAME}
WORKDIR /home/${USERNAME}

# 安裝 pyenv
RUN curl https://pyenv.run | bash
RUN echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bashrc && \
    echo 'export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bashrc && \
    echo 'eval "$(pyenv init --path)"' >> ~/.bashrc && \
    echo 'eval "$(pyenv init -)"' >> ~/.bashrc

# 安裝 Python 3.6.9
RUN /bin/bash -c "source ~/.bashrc && \
    pyenv install 3.6.9 && \
    pyenv global 3.6.9"

# 安裝 poetry
RUN curl -sSL https://install.python-poetry.org | python3 - && \
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc

# 配置 poetry
RUN /bin/bash -c "source ~/.bashrc && \
    poetry config virtualenvs.in-project true"

# 切換回 root 用戶完成剩餘設置
USER root

# 創建工作目錄
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