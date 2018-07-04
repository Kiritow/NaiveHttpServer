helper.print("<h1>Hello World!</h1>")
for k,v in pairs(request) do
    pcall(function() helper.print("<p><b>" .. k .. "</b>\t" .. v .. "</p>") end)
end
helper.print("<h2>Request Parameters</h2>")
if(request.method=="GET") then
    for k,v in pairs(request.param) do
        pcall(function() helper.print("<p><b>" .. k .. "</b>\t" .. v .. "</p>") end)
    end
else
    pcall(function() helper.print("<p>" .. request.param .. "</p>") end)
end