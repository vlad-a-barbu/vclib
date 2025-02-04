module S = Net.Scheduler

let countdown n =
  let rec go n =
    if n = 0
    then ()
    else (
      print_endline @@ string_of_int n;
      S.yield ();
      go (n - 1))
  in
  fun () -> go n
;;

let () =
  S.run
  @@ fun () ->
  S.fork @@ countdown 3;
  S.fork @@ countdown 3
;;
