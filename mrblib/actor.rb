class Actor
  class Error < StandardError; end
  class ProtocolError < Error; end

  def initialize(options = {})
    @dealer = CZMQ::Zsock.new ZMQ::DEALER
    @push = CZMQ::Zsock.new ZMQ::PUSH
    @actor_message = ActorMessage.new
    @name = options.fetch(:name) {String(object_id)}
    @zactor = CZMQ::Zactor.new(ZACTOR_FN, @name)
    router_endpoint = options.fetch(:router_endpoint) {"inproc://#{@name}_router"}
    @zactor.sendx("BIND ROUTER", router_endpoint)
    if @zactor.wait == 0
      @router_endpoint = CZMQ::Zframe.recv(@zactor).to_str
      @dealer.connect(@router_endpoint)
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind router to #{router_endpoint}")
    end
    pull_endpoint = options.fetch(:pull_endpoint) {"inproc://#{@name}_pull"}
    @zactor.sendx("BIND PULL", pull_endpoint)
    if @zactor.wait == 0
      @pull_endpoint = CZMQ::Zframe.recv(@zactor).to_str
      @push.connect(@pull_endpoint)
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pull to #{pull_endpoint}")
    end
    pub_endpoint = options.fetch(:pub_endpoint) {"inproc://#{@name}_pub"}
    @zactor.sendx("BIND PUB", pub_endpoint)
    if @zactor.wait == 0
      @pub_endpoint = CZMQ::Zframe.recv(@zactor).to_str
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pub to #{pub_endpoint}")
    end
    @remote_dealers = {}
    @remote_pushs = {}
  end

  def bind_router(endpoint)
    @zactor.sendx("BIND ROUTER", endpoint)
    if @zactor.wait == 0
      CZMQ::Zframe.recv(@zactor).to_str
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind router to #{endpoint}")
    end
  end

  def bind_pull(endpoint)
    @zactor.sendx("BIND PULL", endpoint)
    if @zactor.wait == 0
      CZMQ::Zframe.recv(@zactor).to_str
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pull to #{endpoint}")
    end
  end

  def bind_pub(endpoint)
    @zactor.sendx("BIND PUB", endpoint)
    if @zactor.wait == 0
      CZMQ::Zframe.recv(@zactor).to_str
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pub to #{endpoint}")
    end
  end

  def zyre_endpoint=(endpoint)
    @zactor.sendx("ZYRE SET ENDPOINT", endpoint)
    if @zactor.wait == 1
      raise Error, "could not bind zyre endpoint to #{endpoint}"
    end
  end

  def zyre_gossip_bind(endpoint)
    @zactor.sendx("ZYRE GOSSIP BIND", endpoint)
    self
  end

  def zyre_gossip_connect(endpoint)
    @zactor.sendx("ZYRE GOSSIP CONNECT", endpoint)
    self
  end

  def zyre_start
    @zactor.sendx("ZYRE START")
    if @zactor.wait == 1
      raise Error, "could not start zyre"
    end
    self
  end

  def load_irep_file(file)
    @zactor.sendx("LOAD IREP FILE", file)
    if @zactor.wait == 1
      raise Error, "could not load irep file #{file}"
    end
    self
  end

  def init(mrb_class, *args)
    @actor_message.id = ActorMessage::INITIALIZE
    @actor_message.mrb_class = String(mrb_class)
    @actor_message.args = args.to_msgpack
    @actor_message.send(@dealer)
    @actor_message.recv(@dealer)
    case @actor_message.id
    when ActorMessage::INITIALIZE_OK
      Proxy.new(self, @actor_message.object_id)
    when ActorMessage::ERROR
      raise @actor_message.mrb_class.constantize, @actor_message.error
    else
      raise ProtocolError, "Invalid Message recieved"
    end
  end

  def send(object_id, method, *args)
    @actor_message.id = ActorMessage::SEND_MESSAGE
    @actor_message.object_id = Integer(object_id)
    @actor_message.method = String(method)
    @actor_message.args = args.to_msgpack
    @actor_message.send(@dealer)
    @actor_message.recv(@dealer)
    case @actor_message.id
    when ActorMessage::SEND_OK
      MessagePack.unpack(@actor_message.result)
    when ActorMessage::ERROR
      raise @actor_message.mrb_class.constantize, @actor_message.error
    else
      raise ProtocolError, "Invalid Message recieved"
    end
  end

  def async_send(object_id, method, *args)
    @actor_message.id = ActorMessage::ASYNC_SEND_MESSAGE
    @actor_message.object_id = Integer(object_id)
    @actor_message.create_uuid
    @actor_message.method = String(method)
    @actor_message.args = args.to_msgpack
    @actor_message.send(@push)
    @actor_message.uuid.dup
  end

  def remote_actors
    @zactor.sendx("GET REMOTE ACTORS")
    if @zactor.wait == 0
      MessagePack.unpack(CZMQ::Zframe.recv(@zactor).to_str(true))
    else
      raise Error, "could not dump state"
    end
  end

  def remote_init(name, mrb_class, *args)
    dealer = @remote_dealers.fetch(name) do
      remote_actor = remote_actors.fetch(name)
      dealer = CZMQ::Zsock.new ZMQ::DEALER
      dealer.sndtimeo = 10000
      dealer.rcvtimeo = 10000
      dealer.connect(remote_actor[:headers]["mrb-actor-v1-router"])
      @remote_dealers[name] = dealer
      dealer
    end
    @actor_message.id = ActorMessage::INITIALIZE
    @actor_message.mrb_class = String(mrb_class)
    @actor_message.args = args.to_msgpack
    @actor_message.send(dealer)
    @actor_message.recv(dealer)
    case @actor_message.id
    when ActorMessage::INITIALIZE_OK
      RemoteProxy.new(self, name, @actor_message.object_id)
    when ActorMessage::ERROR
      raise @actor_message.mrb_class.constantize, @actor_message.error
    else
      raise ProtocolError, "Invalid Message recieved"
    end
  rescue Errno::EWOULDBLOCK, Errno::EAGAIN => e
    @remote_dealers.delete(name)
    raise e
  end

  def remote_send(name, object_id, method, *args)
    dealer = @remote_dealers.fetch(name) do
      remote_actor = remote_actors.fetch(name)
      dealer = CZMQ::Zsock.new ZMQ::DEALER
      dealer.connect(remote_actor[:headers]["mrb-actor-v1-router"])
      @remote_dealers[name] = dealer
      dealer
    end
    @actor_message.id = ActorMessage::SEND_MESSAGE
    @actor_message.object_id = Integer(object_id)
    @actor_message.method = String(method)
    @actor_message.args = args.to_msgpack
    @actor_message.send(dealer)
    @actor_message.recv(dealer)
    case @actor_message.id
    when ActorMessage::SEND_OK
      MessagePack.unpack(@actor_message.result)
    when ActorMessage::ERROR
      raise @actor_message.mrb_class.constantize, @actor_message.error
    else
      raise ProtocolError, "Invalid Message recieved"
    end
  rescue Errno::EWOULDBLOCK, Errno::EAGAIN => e
    @remote_dealers.delete(name)
    raise e
  end

  def remote_async_send(name, object_id, method, *args)
    push = @remote_pushs.fetch(name) do
      remote_actor = remote_actors.fetch(name)
      push = CZMQ::Zsock.new ZMQ::PUSH
      push.sndtimeo = 10000
      push.connect(remote_actor[:headers]["mrb-actor-v1-pull"])
      @remote_pushs[name] = push
      push
    end
    @actor_message.id = ActorMessage::ASYNC_SEND_MESSAGE
    @actor_message.object_id = Integer(object_id)
    @actor_message.create_uuid
    @actor_message.method = String(method)
    @actor_message.args = args.to_msgpack
    @actor_message.send(push)
    @actor_message.uuid.dup
  rescue Errno::EWOULDBLOCK, Errno::EAGAIN => e
    @remote_pushs.delete(name)
    raise e
  end

  class Proxy
    attr_reader :object_id

    def initialize(actor, object_id)
      @actor = actor
      @object_id = object_id
    end

    def send(m, *args)
      @actor.send(@object_id, m, *args)
    end

    def async_send(m, *args)
      @actor.async_send(@object_id, m, *args)
    end
  end

  class RemoteProxy
    attr_reader :object_id

    def initialize(actor, name, object_id)
      @actor = actor
      @name = name
      @object_id = object_id
    end

    def send(m, *args)
      @actor.remote_send(@name, @object_id, m, *args)
    end

    def async_send(m, *args)
      @actor.remote_async_send(@name, @object_id, m, *args)
    end
  end
end
