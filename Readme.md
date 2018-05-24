# HttpServer

A Modern C++ HTTP Serevr Implemention

用现代C++实现的一个http服务器

## 功能

**支持的HTTP方法**:

> GET
>
> POST (POST静态资源会返回405 Method Not Allowed)

**支持静态资源**, Range请求头暂不支持, 数据压缩暂不支持.

**支持Lua作为服务器端脚本执行**.

## 配置与使用

### 配置

端口号和服务器根目录暂不支持动态指定, 可以通过修改[config.h](config.h)来配置.

### 编译

Linux下: 调用`python build.py`进行编译. 编译输出文件为`main`.

Windows下: 如果安装并配置了g++可以使用`build.py`脚本进行编译. 否则需要建立VS项目.
