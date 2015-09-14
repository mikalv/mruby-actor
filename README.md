# mruby-actor

Examples
========

worker.rb:
```ruby
class Worker
  def heavy_lifting
    CZMQ::Zclock.sleep 5000
    puts "finished"
  end
end
```
---------
```shell
mruby/bin/mrbc worker.rb
```
--------
```ruby
actor = Actor.new

actor.load_irep_file('worker.mrb')

worker = actor.init('Worker')

worker.send(:heavy_lifting)

worker.async_send(:heavy_lifting)

```
