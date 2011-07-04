require 'mkmf'
if RUBY_VERSION[0,3] == '1.8'
  puts "-I#{Config::expand(CONFIG['archdir'])}"
else
  puts "-I#{Config::expand(CONFIG['rubyhdrdir'])} -I#{Config::expand(CONFIG['rubyhdrdir'])}/#{Config::expand(CONFIG['arch'])}"
end
