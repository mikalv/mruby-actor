class Actor
  class Error < StandardError; end
  class ProtocolError < Error; end

  attr_reader :zactor, :pub_endpoint

  def initialize(options = {})
    @dealer = CZMQ::Zsock.new ZMQ::DEALER
    @push = CZMQ::Zsock.new ZMQ::PUSH
    @actor_message = ActorMessage.new
    @zactor = CZMQ::Zactor.new(ZACTOR_FN, options[:mrb_file])
    @address = options.fetch(:address) {String(object_id)}
    router_endpoint = options.fetch(:router_endpoint) {"inproc://#{@address}_router"}
    @zactor.sendx("BIND ROUTER", router_endpoint)
    if @zactor.wait == 0
      @router_endpoint = CZMQ::Zframe.recv(@zactor).to_str
      @dealer.connect(@router_endpoint)
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind router to #{router_endpoint}")
    end
    pull_endpoint = options.fetch(:pull_endpoint) {"inproc://#{@address}_pull"}
    @zactor.sendx("BIND PULL", pull_endpoint)
    if @zactor.wait == 0
      @pull_endpoint = CZMQ::Zframe.recv(@zactor).to_str
      @push.connect(@pull_endpoint)
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pull to #{pull_endpoint}")
    end
    pub_endpoint = options.fetch(:pub_endpoint) {"inproc://#{@address}_pub"}
    @zactor.sendx("BIND PUB", pub_endpoint)
    if @zactor.wait == 0
      @pub_endpoint = CZMQ::Zframe.recv(@zactor).to_str
    else
      errno = Integer(CZMQ::Zframe.recv(@zactor).to_str(true))
      raise SystemCallError._sys_fail(errno, "could not bind pub to #{pub_endpoint}")
    end
  end

  def load_string(string)
    @zactor.sendx("LOAD STRING", string)
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
end
