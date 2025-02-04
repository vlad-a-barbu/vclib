module S = Scheduler

let rec handler fd create_response =
  let finally () = Unix.close fd in
  Fun.protect ~finally
  @@ fun () ->
  match Tcp.read_nb fd with
  | Error WouldBlock ->
    S.yield ();
    handler fd create_response
  | Ok request -> Tcp.write fd @@ create_response request
;;

let serve ?(port = 3000) ?(backlog = 10) create_response =
  let fd = Tcp.listen ~port ~backlog in
  Printf.printf "listening on port %d\n%!" port;
  let rec go () =
    let fd, _ = Unix.accept fd in
    S.fork (fun () -> handler fd create_response);
    go ()
  in
  S.run go
;;
