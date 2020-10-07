
include("./../interfaces/julia/libcdi.jl")
using .cdi
using .cdi.ZAXIS
using .cdi.GRID
using .cdi.TIME
using .cdi.TAXIS
using .cdi.FileTypes

println("cdi_write example has started")

const lons = [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330]
const lats = [-75, -45, -15, 15, 45, 75]
const levs = [101300, 92500, 85000, 50000, 20000]
const nts = 3 # number of timestamps

# Init var1 and var2
const var1 = fill(1.1, length(lons) * length(lats))
const var2 = fill(2.2, length(lons) * length(lats) * length(levs))

# Create a regular lon/lat grid
gridID = gridCreate(GRID.LONLAT, length(lons) * length(lats))
gridDefXsize(gridID, length(lons))
gridDefYsize(gridID, length(lats))
gridDefXvals(gridID, lons)
gridDefYvals(gridID, lats)

# Create a surface level Z-axis
zaxisID1 = zaxisCreate(ZAXIS.SURFACE, 1)

# Create a pressure level Z-axis
zaxisID2 = zaxisCreate(ZAXIS.PRESSURE, length(levs))
zaxisDefLevels(zaxisID2, levs)

# Create a variable list
vlistID = vlistCreate()

# Define the variables
varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME.VARYING)
varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME.VARYING)

# Define the variable names
vlistDefVarName(vlistID, varID1, "varname1")
vlistDefVarName(vlistID, varID2, "varname2")

# Create a Time axis
taxisID = taxisCreate(TAXIS.ABSOLUTE)

# Assign the Time axis to the variable list
vlistDefTaxis(vlistID, taxisID)

# Create a dataset in netCDF format
streamID = streamOpenWrite("example.nc", FileTypes.FILETYPE_NC);
if streamID < 0
    println(cdiStringError(streamID))
    exit(1)
end

# Assign the variable list to the dataset
streamDefVlist(streamID, vlistID);

# Loop over the number of time steps
for tsID in 0:nts
    # Set the verification date to 1985-01-01 + tsID
    taxisDefVdate(taxisID, 19850101+tsID);

    # Set the verification time to 12:00:00
    taxisDefVtime(taxisID, 120000);

    # Define the time step
    streamDefTimestep(streamID, tsID);

    # Write var1 and var2
    streamWriteVar(streamID, varID1, var1, 0);
    streamWriteVar(streamID, varID2, var2, 0);
end

# Close the output stream
streamClose(streamID);

# Destroy the objects
vlistDestroy(vlistID);
taxisDestroy(taxisID);
zaxisDestroy(zaxisID1);
zaxisDestroy(zaxisID2);
gridDestroy(gridID);