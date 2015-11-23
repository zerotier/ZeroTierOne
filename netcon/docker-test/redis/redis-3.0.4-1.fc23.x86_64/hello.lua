local msg = "welcome to the machine!"
redis.call("SET", "msg", msg)
return redis.call("GET", "msg")
