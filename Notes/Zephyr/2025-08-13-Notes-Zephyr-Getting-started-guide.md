# 入门指南

**文档:** https://docs.zephyrproject.org/latest/introduction/index.html

## 安装 APT 依赖项

    ```
    sudo apt install --no-install-recommends git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
    xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
    ```

## 安装 Python 依赖项

```
mkdir ~/zephyr_workspace/
mkdir ~/zephyr_workspace/zephyr_main

# Create a new virtual environment
python3 -m venv ~/zephyr_workspace/zephyr_main/.venv

# Once activated your shell will be prefixed with (.venv)
source ~/zephyr_workspace/zephyr_main/.venv/bin/activate

# Install west
pip install west
```

## 获取 Zephyr 源码

```
# Get the Zephyr manifest
west init ~/zephyr_workspace/zephyr_main/

cd ~/zephyr_workspace/zephyr_main/

# Get the Zephyr source code
west update

# Export a Zephyr CMake package
west zephyr-export

# Install the west extension command
west packages pip --install
```

## 安装 SDK (Toolchains)

```
west sdk install
```

默认的安装位置为 **\$HOME\/zephyr\-sdk\-\<version\>**

编译时，构建系统会自动从以下位置获取工具链:

 - **$HOME**

 - **$HOME/.local**

 - **$HOME/.local/opt**

 - **$HOME/bin**

 - **/opt**

 - **/usr/local**

如果希望安装到其它位置，则需要使用环境变量 **ZEPHYR_SDK_INSTALL_DIR** 配置安装目录。

## 编译并运行

```
cd ~/zephyr_workspace/zephyr_main/zephyr/samples/hello_world/

west build -b native_sim .

west build -t run

# or run zephyr.exe directly:
./build/zephyr/zephyr.exe
```
