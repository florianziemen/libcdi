#inputpath = "/home/mpim/m300859/libcdi/interfaces/julia/cdi_macro.h"
inputpath = "/home/mpim/m300859/libcdi/src/cdi.h"

io = open(inputpath, "r")
txt = read(io, String)
lines = split(txt, '\n')

res = Vector{Tuple{String, String, String}}(undef, 0)

for line in lines
    if length(line) > 0 && line[1] == '#'
        tokens = filter(x -> !isempty(x), split(line, ' '))

        if length(tokens) < 3 || tokens[1] != "#define"
            continue
        end

        comment = ""
        if length(tokens) > 4
            for c in tokens[5:end]
                comment = comment * " " * c
            end
        end

        println(tokens)
        println(comment)

        append!(res, [(tokens[2], tokens[3], comment)])
        
        #println(tokens)
    end
end

close(io)

if length(res) == 0
    exit(0)
end

io = open("/home/mpim/m300859/libcdi/interfaces/julia/cdi_macro.jl", "w")
write(io, "@enum enum::Cint begin\n")
for (name, value, comment) in res 
    write(io, "  $name = $value # $comment\n")
end
write(io, "end\n")
close(io)