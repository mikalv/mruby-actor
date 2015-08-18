class Actor
  class Error < StandardError; end
  class ProtocolError < Error; end

  def init(mrb_class, *args)
    @actor_message.id = ActorMessage::INITIALIZE
    @actor_message.mrb_class = String(mrb_class)
    if (args.count > 0)
      @actor_message.args = args.to_msgpack
    end
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
    if (args.count > 0)
      @actor_message.args = args.to_msgpack
    end
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
    @actor_message.uuid = RandomBytes.buf CZMQ::ZUUID_LEN
    @actor_message.object_id = Integer(object_id)
    @actor_message.method = String(method)
    if (args.count > 0)
      @actor_message.args = args.to_msgpack
    end
    @actor_message.send(@push)
    self
  end

  class Proxy
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
