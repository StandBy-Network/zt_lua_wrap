--[[ MIT License

Copyright (c) 2020 StandBy.Network

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. ]]

-- First receive
--[[ repeat
    size, msg = recv(0X98646573D4)
    print("received " .. msg .. "size " .. tostring(size))
    send(0X98646573D4, "pong")
    print("sent pong")
until size ~= -1 ]]

for i = 0, 10000000, 1 do
    err, err_msg = udp_recv(0XF6EE77EEE7)
    if err then
        if err_msg then
            print("error " .. err_msg)
        else
            print(err)
        end
    end
end

--The other side
--[[ repeat
    send(0xFA2037EAE9, "ping")
    print("sent ping")
    size, msg = recv(0xFA2037EAE9)
    print("received " .. msg .. "size " .. tostring(size))
until size ~= -1 ]]