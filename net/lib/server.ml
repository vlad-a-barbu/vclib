module S = Scheduler

let handler fd create_response =
  let finally () = Unix.close fd in
  let rec go fd create_response =
    match Tcp.read_nb fd with
    | Error WouldBlock ->
      S.yield ();
      go fd create_response
    | Ok request -> create_response request |> Tcp.write fd
  in
  Fun.protect ~finally @@ fun () -> go fd create_response
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
