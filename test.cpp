#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <unordered_map>
#include <algorithm>

using namespace std;

typedef struct order{
	char action;
	int orderid;
	char side;
	int quantity;
	int price;
}order;


class FeedHandler
{
	// keep all pending buy order
	vector<order> buy_orderbook;

	// keep all pending sell order
	vector<order> sell_orderbook;

	// keep order ID / side (B, S) mapping
	unordered_map<int, char>orderid_list;

	// keep bad syntax order 
	vector<string> err_order;

	// current trade price (most recent trade)
	int cur_price;

	// total quantity of current trade price
	int cur_quantity;

	// to parse the message
	inline string tokenizer(string &message);

	// to extract message into order and check invalid message
	int extract_order(string message, order &o);

	// insert to pending order queue in ascending sorting order
	void insert_orderbook(order &o);

	// match the order in pending order queues
	int match_order(order &o);

	// execute the trade print out
	void trade_order(string &str, int price, int quantity);

	// update order if it exists
	int update_order(order &o);

	// remove order if it exists
	int remove_order(order &o);
	
	public:
		FeedHandler();

		// Process current message
		void ProcessMessage(const std::string &line);

		// Print current order book
		void PrintCurrentOrderBook(std::ostream &os) const;

		// Print # of error message and the error messages
		void PrintErrorOrder(std::ostream &os) const;

		// Print current order ID list
		void PrintOrderIDList(std::ostream &os) const;
};

FeedHandler::FeedHandler()
{
	cur_price = -1;
	cur_quantity = -1;
}

inline string FeedHandler::tokenizer(string &message)
{
	int d;

	d = message.find(',');
	if (d == string::npos)
	{
		return message;
	}

	message = message.substr(d+1);
	d = message.find(',');
	if (d == string::npos)
	{
		return message;
	}

	return message.substr(0,d);
}


int FeedHandler::extract_order(string message, order &o)
{
	string token;

	if (message[0] == 'A' || message[0] == 'X' || message[0] == 'M')
	{
		try
		{
			o.action = message[0];
			token = tokenizer(message);
			o.orderid = stoi(token);
			token = tokenizer(message);
			o.side = token[0];
			token = tokenizer(message);
			o.quantity= stoi(token);
			token = tokenizer(message);
			o.price = stoi(token);


			// Error checking
			switch (o.action)
			{
				case 'A':
					// duplicate orderid
					if (orderid_list.find(o.orderid)!=orderid_list.end())
						return -1;
					// negative values
					if (o.price < 0 || o.quantity < 0)
						return -1;
					// invalid side
					if (o.side != 'B' && o.side != 'S')
						return -1;
					
					break;
				case 'X':
					// cannot find orderid
					if (orderid_list.find(o.orderid)==orderid_list.end())
						return -1;
					// negative values
					if (o.price < 0 || o.quantity < 0)
						return -1;
					// invalid side
					if (o.side != 'B' && o.side != 'S')
						return -1;
					break;
				case 'M':
					// cannot find orderid
					if (orderid_list.find(o.orderid)==orderid_list.end())
						return -1;
					// negative values
					if (o.price < 0 || o.quantity < 0)
						return -1;
					// invalid side
					if (o.side != 'B' && o.side != 'S')
						return -1;
					break;
				default:
					return -1;
			}
		}
		catch (...) // catch any invalid inputs
		{
			return -1;
		}
	}
	else
		return -1;

//	cout << "in:" << o.price << " " << o.side << " " << o.quantity << endl;
	
	return 0;
}

void FeedHandler::insert_orderbook(order &o)
{
	vector<order>::iterator it;

	if (o.side == 'B')
	{
		buy_orderbook.push_back(o);
		orderid_list[o.orderid] = o.side;
		sort(buy_orderbook.begin(), buy_orderbook.end(), [](order a, order b){return a.price < b.price;});
	}
	else if (o.side == 'S')
	{
		sell_orderbook.push_back(o);
		orderid_list[o.orderid] = o.side;
		sort(sell_orderbook.begin(), sell_orderbook.end(), [](order a, order b){return a.price < b.price;});
	}
}

void FeedHandler::trade_order(string &str, int price, int quantity)
{
	str.append("T");
	str.append(",");
	str.append(to_string(quantity));
	str.append(",");
	str.append(to_string(price));

	if (cur_price == -1)
	{
		cur_price = price;
		cur_quantity = quantity;
	}
	else if (cur_price != price)
	{
		cur_quantity = quantity;
		cur_price = price;
	}
	else
	{
		cur_quantity+=quantity;
	}

	str.append(" ");
	str.append("=");
	str.append(">");
	str.append(" ");
	str.append(to_string(cur_quantity));
	str.append("@");
	str.append(to_string(cur_price));

	cout << str << endl;
	str.clear();

	return;
}

int FeedHandler::match_order(order &o)
{
	vector<order>::iterator it;
	string str;

	if (o.side == 'B')
	{
		for (it=sell_orderbook.begin(); it!=sell_orderbook.end(); ++it)
		{
			if (it->price <= o.price)
			{
				if(it->quantity >= o.quantity)
				{
					trade_order(str, it->price, o.quantity);
					it->quantity-=o.quantity;
					o.quantity = 0;
					if (it->quantity == 0)
					{
						orderid_list.erase(it->orderid);
						sell_orderbook.erase(it);
						if (it == sell_orderbook.end())
							break;
					}
				}
				else 
				{
					trade_order(str, it->price, it->quantity);
					o.quantity-=it->quantity;
					orderid_list.erase(it->orderid);
					sell_orderbook.erase(it);
					if (it == sell_orderbook.end())
						break;
				}
			}
		}
	}
	else if (o.side == 'S')
	{
		for (it=buy_orderbook.begin(); it!=buy_orderbook.end(); ++it)
		{
			if (it->price >= o.price)
			{
				if(it->quantity >= o.quantity)
				{
					trade_order(str, o.price, o.quantity);
					it->quantity-=o.quantity;
					o.quantity = 0;
					if (it->quantity == 0)
					{
						orderid_list.erase(it->orderid);
						buy_orderbook.erase(it);
						if (it == buy_orderbook.end())
							break;
					}
				}
				else 
				{
					trade_order(str, o.price, it->quantity);
					o.quantity-=it->quantity;
					orderid_list.erase(it->orderid);
					buy_orderbook.erase(it);
					if (it == buy_orderbook.end())
						break;
				}
			}
		}
	}
	
	if (o.quantity)
		return 0;
	else
		return -1;
}

int FeedHandler::remove_order(order &o)
{
	vector<order>::iterator it;
	char side = orderid_list[o.orderid];
	orderid_list.erase(o.orderid);

	if (side == 'B')
	{
		for (it=buy_orderbook.begin(); it!=buy_orderbook.end(); ++it)
		{
			if (it->orderid == o.orderid)
			{
				buy_orderbook.erase(it);
				if (it == buy_orderbook.end())
					break;
			}
		}
	}
	else if (side == 'S')
	{
		for (it=sell_orderbook.begin(); it!=sell_orderbook.end(); ++it)
		{
			if (it->orderid == o.orderid)
			{
				sell_orderbook.erase(it);
				if (it == sell_orderbook.end())
					break;
			}
		}
	}

	return 0;
}


int FeedHandler::update_order(order &o)
{
	remove_order(o);
 	if (match_order(o)==0)
		insert_orderbook(o);
 
	return 0;
}


void FeedHandler::ProcessMessage(const std::string &line)
{
	order o;

	if (extract_order(line,o))
		err_order.push_back(line);
	else // if order is good
	{
		switch(o.action)
		{
			case 'A':
				if (match_order(o)==0)
					insert_orderbook(o);
				break;
			case 'X':
				remove_order(o);
				break;
			case 'M':
				update_order(o);
				break;
			default:
				break;
		}
	}
}

void FeedHandler::PrintCurrentOrderBook(std::ostream &os) const
{
	vector<order>::const_reverse_iterator it;
	int price=-1;
	string order_entry;

	os << endl;
	os << "Order Book:" << endl;
	os << "==========" << endl;

	for (it=sell_orderbook.rbegin(); it!=sell_orderbook.rend(); ++it)
	{
		if (price == it->price)
		{
			string str;
			str.push_back(it->side);
			str.append(" ");
			str.append(to_string(it->quantity));
			str.append(" ");
			order_entry.insert(0,str);
		}
		else
		{
			if (price!=-1)
			{
				os << price << " " << order_entry << endl;
				order_entry.clear();
			}
			order_entry.push_back(it->side);
			order_entry.append(" ");
			order_entry.append(to_string(it->quantity));
			price = it->price;
		}

	}
	if (order_entry.length())
		os << price << " " << order_entry << endl;
	os << endl;

	order_entry.clear();
	price = -1;
	for (it=buy_orderbook.rbegin(); it!=buy_orderbook.rend(); ++it)
	{
		if (price == it->price)
		{
			string str;
			str.push_back(it->side);
			str.append(" ");
			str.append(to_string(it->quantity));
			str.append(" ");
			order_entry.insert(0,str);
		}
		else
		{
			if (price!=-1)
			{
				os << price << " " << order_entry << endl;
				order_entry.clear();
			}
			order_entry.push_back(it->side);
			order_entry.append(" ");
			order_entry.append(to_string(it->quantity));
			price = it->price;
		}
	}
	if (order_entry.length())
		os << price << " " << order_entry << endl;
	os << "==========" << endl;

	return;
}

void FeedHandler::PrintErrorOrder(std::ostream &os) const
{
	vector<string>::const_iterator it;

	os << endl;
	os << "Error order: " << err_order.size() << endl;
	for (it=err_order.begin(); it!=err_order.end(); ++it)
	{
		os << *it << endl;
	}
	os << endl;
}

void FeedHandler::PrintOrderIDList(std::ostream &os) const
{
	os << endl;
	os << "order ID list: ";

	for (auto& x: orderid_list)
		os << " " << x.first;
	os << endl;

	return;
}


int main(int argc, char **argv)
{
	FeedHandler feed;
	std::string line;
	const std::string filename(argv[1]);
	std::ifstream infile(filename.c_str(), ios::in);
	int counter = 0;
	while (std::getline(infile, line)) {
		feed.ProcessMessage(line);
		if (++counter % 10 == 0) {
			feed.PrintCurrentOrderBook(std::cerr);
		}
	}
	feed.PrintCurrentOrderBook(std::cout);
	feed.PrintErrorOrder(std::cout);
	feed.PrintOrderIDList(std::cout);
	return 0;
}


