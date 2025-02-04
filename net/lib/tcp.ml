let listen ~port ~backlog =
  let open Unix in
  let fd = socket PF_INET SOCK_STREAM 0 in
  setsockopt fd SO_REUSEADDR true;
  bind fd @@ ADDR_INET (inet_addr_any, port);
  listen fd backlog;
  fd
;;

let read ?(buff_len = 0x1000) fd =
  let buff = Bytes.create buff_len in
  let rec go acc =
    let n = Unix.read fd buff 0 buff_len in
    if n = 0
    then List.rev acc
    else (
      let chunk = Bytes.sub buff 0 n in
      go (chunk :: acc))
  in
  Bytes.concat Bytes.empty @@ go []
;;

type read_nb_error = WouldBlock

let read_nb ?(timeout_s = 0.01) fd =
  match Unix.select [ fd ] [] [] timeout_s with
  | fd :: _, [], [] -> Ok (read fd)
  | _ -> Error WouldBlock
;;

let write fd buff =
  let len = Bytes.length buff in
  let rec go left =
    let pos = len - left in
    let n = Unix.write fd buff pos left in
    if n = left then () else go (left - n)
  in
  go len
;;
