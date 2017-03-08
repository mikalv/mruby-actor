MRuby::Gem::Specification.new('mruby-actor') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'actor library for mruby'
  spec.add_dependency 'mruby-zmq'
  spec.add_dependency 'mruby-zyre'
end
