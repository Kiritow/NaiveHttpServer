helper.print("<h1>Hello World!</h1>")
for k,v in pairs(request) do
    helper.print("<p><b>" .. k .. "</b>\t" .. v .. "</p>")
end
