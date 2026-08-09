
Challenge: organising the data to contain all the information.
- If we want a really light points struct, we can have stuff like point outcome which is an enum - ace, ufe, fe, etc.
- And then we can have a field that has 1 if player 1 won the point or 2 if player 2 won the point.
- Thus we implicitly hold the data (player 1 scored a winner) if outcome = winner and point winner = player 1.
- That's fine but that also means that if we want to find out how many winners player 1 has scored, we either need to add that to a "player 1" object of some sort whilst parsing the data, or calculate from every single point object.
- I think with this example, having a player 1 object is clearly the better solutionr?
    - However in the future we might want to have player 1, match: match_id (which can be broken down into tourno and surface), how many unforced errors they usually make and how many winners they usually score
    - So their number of winners might have to be in a nested structure? Or is this where the database stuff comes in?
- What about if I want to be able to eventually calculate stats on "net points won"?
    - as part of the struct each point can have a bool whether it was a net point? or maybe two bools for P1_net and P2_net (ie. who initiated the 'net' part of it)
    - and then we use the point winner and outcome to find out how the net point ended (don't use their P1NetPointWon because I don't know how they derived their data)
    - What does this mean for how we are going to structure our point struct?

Decision:
    - Each point is an event that only holds data for that point
    - Persistence of data will come at a later stage when databases come into the picture but isn't yet required for the MVP
    - We are not going to store derived fields like P1NetPointWon
    - We are going to use two booleans p1_net_point and p2_net_point

