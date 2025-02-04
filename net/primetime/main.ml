module Srv = Net.Server

let create_response request = request
let () = Srv.serve create_response
