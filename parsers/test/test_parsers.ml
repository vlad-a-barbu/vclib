type 'a test =
  { name : string
  ; fn : unit -> 'a
  ; expected : 'a
  ; to_string : 'a -> string
  }

let success test = Printf.sprintf "[%s] SUCCESS" test.name

let failure ?actual ?exn test =
  let expected = test.to_string test.expected in
  let actual =
    match actual with
    | Some x -> test.to_string x
    | None -> ""
  in
  let exn =
    match exn with
    | Some x -> Printexc.to_string x
    | None -> ""
  in
  Printf.sprintf
    "[%s] FAILURE\n\texpected '%s'\n\tactual '%s%s'"
    test.name
    expected
    actual
    exn
;;

let run_tests tests =
  let rec go acc = function
    | [] -> List.rev acc
    | test :: tl ->
      (try
         let actual = test.fn () in
         let msg =
           if actual = test.expected then success test else failure ~actual test
         in
         go (msg :: acc) tl
       with
       | exn -> go (failure ~exn test :: acc) tl)
  in
  go [] tests
;;

let json_of_string_tests () =
  let open Parsers.Json in
  let json str = fun () -> of_string str in
  [ { name = "Integer"; fn = json "3"; expected = Number (Int 3); to_string = show }
  ; { name = "Float"; fn = json "3.33"; expected = Number (Float 3.33); to_string = show }
  ; { name = "String"
    ; fn = json "\"works\""
    ; expected = String "works"
    ; to_string = show
    }
  ]
;;

let () =
  print_endline "";
  json_of_string_tests ()
  |> run_tests
  |> List.iteri @@ fun i msg -> Printf.printf "%d) %s\n\n%!" i msg
;;
