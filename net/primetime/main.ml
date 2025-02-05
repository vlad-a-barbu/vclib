module Srv = Net.Server

(* todo after finishing the json parser *)
let create_response request = request
let () = Srv.serve create_response
