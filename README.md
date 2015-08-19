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
```ruby
actor = Actor.new('worker.rb', 'inproc://worker_router', 'inproc://worker_pull', 'tcp://127.0.0.1:*')

worker = actor.init('Worker')

worker.send(:heavy_lifting)

worker.async_send(:heavy_lifting)
```
