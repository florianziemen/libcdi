require 'rbconfig'
puts Config::expand(Config::MAKEFILE_CONFIG["archdir"])
