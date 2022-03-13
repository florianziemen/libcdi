require 'CdiLib'
include CdiLib
ifile = ARGV[0].nil? ? "../testdata/mulval.nc" : ARGV[0]
streamID = streamOpenRead(ifile)
exit 1 if (streamID < 0)
vlistID  = streamInqVlist(streamID)
nvars    = vlistNvars(vlistID)
(0...nvars).each {|i|
  print vlistInqVarCode(vlistID, i).to_s + ' '
}
puts
streamClose(streamID)
