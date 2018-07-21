Assumption:
- All messages are in action,orderid,side,quantity,price format.  All other formats are treated as bad inputs
- If new order can be matched to pending orders, it will print T,quantity,price=>total_quantity@current_price format.  The corresponding pending orders will be executed and removed.
- At the end error orders and pending order ID list will be printed out

