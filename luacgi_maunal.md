# LuaCGI Maunal

当Lua脚本被请求时将启动Lua虚拟机加载代码并运行.

如果代码有错误或运行期间未正常结束,服务器将返回500到客户端.

## 预定义变量

以下变量是预定义的, lua脚本可以直接使用

```lua
request 请求数据表
request.method 请求方法(GET,POST)
request.http_version http版本(HTTP/1.1)
request[...] 其他HTTP请求头数据
    例如 request["Content-Length"]
request.param URL参数表
    例如对于 http://localhost/index.lua?hello=world
    request.param["hello"]值为"world"
```

```lua
response 响应数据表
response.output 响应数据,将作为http响应正文发送到客户端
response[...] 其他http响应头
    例如response["Cookie"]="hello=world"
    在响应头中会有Cookie: hello=world,并在客户端设置一个名为hello值为world的Cookie.
```

```lua
helper 帮助函数表
helper.print 与print函数使用方法相同,但输出内容会附加到response.output中
```

