####### local Types ######

@enum ReadState begin
    waitUntilEnum
    readEnumValues
end

struct rawEnumValue
    name::String
    value::String
    comment::String
end

mutable struct rawEnum 
    name::String
    values::Vector{rawEnumValue}
end

function rawEnum()
    rawEnum("", Vector{rawEnumValue}(undef, 0))
end

####### local functions ########
function readEnumValue(tokens::Vector{String})::rawEnumValue
    if length(tokens) < 3 || tokens[1] != "#define"
        return
    end

    comment = join(tokens[5:end], " ")

    return rawEnumValue(tokens[2], tokens[3], comment)
end

function macroName(name::AbstractString)
    nameparts = split(name, "_")
    res = name
    if length(nameparts) == 2
        res = nameparts[2]
    elseif length(nameparts) > 2
        res = join(nameparts[2:end], "_")
    end
    return res
end

###### Main #######

#inputpath = joinpath(dirname("./../../"), "src", "cdi.h") |> normpath
inputpath = "/home/mpim/m300859/libcdi/src/cdi.h"

# Read input file
io = open(inputpath, "r")
lines = split(read(io, String), '\n') 
close(io)

enums = Vector{rawEnum}(undef, 0)
readState = waitUntilEnum
tmpEnum = rawEnum()

for line in lines
    trimedline = line |> lstrip |> rstrip

    # skip empty lines
    if length(trimedline) == 0
        continue
    end

    # create clean token stream from line
    tokens = filter(x -> !isempty(x), split(trimedline, ' '))

    if tokens[1] == "//" && tokens[2] == "#BEGIN#" && readState == waitUntilEnum
        global tmpEnum.name = tokens[3]
        global readState = readEnumValues
    elseif tokens[1] == "//" && tokens[2] == "#BEGIN#" && readState == readEnumValues
        println("Sytax Error open enum block twice")
        return
    elseif readState == readEnumValues && tokens[1] == "#define"
        append!(tmpEnum.values, [readEnumValue(convert(Vector{String}, tokens))])
    elseif tokens[1] == "//" && tokens[2] == "#END#" && readState == waitUntilEnum
        println("Sytax Error close enum block without open one")
        return
    elseif readState == readEnumValues && tokens[1] == "//" && tokens[2] == "#END#"
        append!(enums, [tmpEnum])
        global readState = waitUntilEnum
        global tmpEnum = rawEnum()
    end
end

println(enums)
if length(enums) == 0
    exit(0)
end

io = open("/home/mpim/m300859/libcdi/interfaces/julia/cdi_macro.jl", "w")
write(io, "using CEnum #redefine enum elements with the same value\n\n")
for e in enums
    # create namespace
    write(io, "module _$(e.name)\n")

    #create enum
    write(io, "@enum $(e.name)::Cint begin\n")
    for v in (e.values)
        name = macroName(v.name)

        if v.comment == ""
            write(io, "    $name = $(v.value)\n")
        else
            write(io, "    $name = $(v.value) # $(v.comment)\n")
        end
    end
    write(io, "end # End of Enum $(e.name)\n")
    
    #export enum to namespace
    write(io, "export $(e.name)\n")

    #export enum values to namespace
    if length(e.values) == 1
        write(io, "export " * macroName(e.values[1].name) * "\n")
    elseif length(e.values) > 1
        write(io, "export " * macroName(e.values[1].name))
        for v in e.values[2:end]
            name = macroName(v.name)
            write(io, ", $name")
        end
        write(io, "\n")
    end

    write(io, "end # End of Module _$(e.name)\n")
    write(io, "using ._$(e.name)")
    write(io, "\n\n")
end

close(io)