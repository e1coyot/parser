#include <string>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <cstdio>

using namespace std;

struct Token {
	enum Type { value, operation, opening_bracket, closing_bracket};
	Type type;
	string text;
};

struct Tokenizer {
	//I am too lazy to write real tokenizer, it is very simple. I hope formula generator for fake tokenizer will be ok. 
public:
	Tokenizer() { content=generate(); pos=content.begin(); } ;
	const Token* peek() { return pos!=content.end() ?&*pos:0; } ;
	const Token* get() { if (pos!=content.end()) { cout << pos->text << " "; } return pos!=content.end()?&*pos++:0; } ;
private:
    vector<Token> generate(int level=0);
    
private:
	vector<Token>::iterator pos;
	vector<Token> content;
};

class Expression;
typedef struct auto_ptr<Expression> ExpressionPtr;

//Represents abstract expression
class Expression {
public:
	Expression() {}
	virtual ~Expression() {}
	//actually this static parse functions could be constructor in most classes. I.e. this is violztion of pronciple 'Resource Acquisition Is Initialization'. 
	//but some fuctions return different classes depending on context, i.e. this is some kind of 'virtual constructor' (see Operation::parse for example) 
	//so I made decision  to make static construction function in all classes, just for uniformity
	static ExpressionPtr parse(Tokenizer& tokens);
	virtual float calc()=0;
	virtual void debug(string prefix)=0;
};


//Represents single value: for example 3.1415
class Value: public Expression {
public:
	Value() {}
	virtual ~Value() {}
	static bool isItYou(Tokenizer& tokens);
	static ExpressionPtr parse(Tokenizer& tokens);
	virtual float calc() { return _value; }
	virtual void debug(string prefix) { cout << prefix << "Value(" <<  calc() <<"): " << _value << endl; } 
protected:
	float _value;
};

//Represents operaion: + or -
class Operation: public Expression {
public:
	Operation() {};
	virtual ~Operation() {}
	static ExpressionPtr parse(Tokenizer& tokens, ExpressionPtr& l);
	virtual float calc();
	virtual void debug(string prefix) { cout << prefix << "Operation(" <<  calc() <<"): " << _operation << endl; if ( _left.get() ) _left->debug(prefix + "\t"); if ( _right.get() ) _right->debug(prefix + "\t"); } 
protected:
	std::auto_ptr<Expression> _left;
	std::auto_ptr<Expression> _right;
	string _operation;
};

//Represents operaion: * or /
class PriorityOperation: public Operation {
public:
	PriorityOperation() {};
	virtual ~PriorityOperation() {}
	static ExpressionPtr parse(Tokenizer& tokens, ExpressionPtr& left);
	//virtual float calc(); inherit it
	virtual void debug(string prefix) { cout << prefix << "PriorityOperation(" <<  calc() <<"): " << _operation << endl; if ( _left.get() ) _left->debug(prefix + "\t"); if ( _right.get() ) _right->debug(prefix + "\t"); } 
};


//Represents bracketed expression: (expr)
class BracketExpression: public Expression {
public:
	static bool isItYou(Tokenizer& tokens);
	static ExpressionPtr parse(Tokenizer& tokens);
	virtual float calc() { return _internal->calc(); } ;
	virtual void debug(string prefix) { cout << prefix << "Brackets(" <<  calc() <<"): "  << endl; _internal->debug(prefix + "\t"); } 
protected:
	std::auto_ptr<Expression> _internal; 
};


ExpressionPtr Expression::parse(Tokenizer& tokens)
{
    //cout << "Expression::parse" << endl;
    
	if (!tokens.peek()) return ExpressionPtr();

	if ( BracketExpression::isItYou(tokens) )
	{
		return BracketExpression::parse(tokens);
	}
	else
	if ( Value::isItYou(tokens) )
	{	
		return Value::parse(tokens);
	}
	else
	{
		throw logic_error("(Expression) Wtf is that: " + tokens.peek()->text );
	}
}

bool Value::isItYou(Tokenizer& tokens) 
{
	const Token* t = tokens.peek();
	if ( !t || t->type != Token::value ) return false; 

	char* endptr;
	strtod( t->text.c_str(), &endptr);
	return *endptr == 0;
}

ExpressionPtr Value::parse(Tokenizer& tokens)
{
    //cout << "Value::parse" << endl;
    
	std::auto_ptr<Value> foo( new Value );

	const Token* t=tokens.get();
	assert( t && t->type == Token::value ); 

	char* endptr;
	foo->_value = strtod( t->text.c_str(), &endptr);
	return ExpressionPtr(foo.release()); //lack of heterosexual auto_ptr conversions is killing me
}

bool BracketExpression::isItYou(Tokenizer& tokens) 
{
	return tokens.peek() && tokens.peek()->type == Token::opening_bracket;
}


ExpressionPtr BracketExpression::parse(Tokenizer& tokens)
{
    //cout << "BracketExpression::parse" << endl;
	const Token* t=tokens.get();
	assert ( t->type == Token::opening_bracket );

	auto_ptr<BracketExpression> result ( new BracketExpression );
	ExpressionPtr null;
	result->_internal = Operation::parse(tokens, null);

	t = tokens.get();
	if ( t ==0 || t->type != Token::closing_bracket )
	{
		throw logic_error("(BracketExpression) mismatched brackets ");
	}

	return ExpressionPtr(result.release());
}


ExpressionPtr Operation::parse(Tokenizer& tokens, ExpressionPtr& l)
{
    //cout << "Operation::parse:" << l.get() << endl;
	ExpressionPtr left;

	if (l.get()) 
	{
		left=l;
		// left is assigned for us.
	}
	else
	{
		left=Expression::parse(tokens);
	}	

	const Token *t=tokens.peek();
	if (!t || t->type == Token::closing_bracket  ) return left; //just return Value, sorry no operation guys

	if (t->type == Token::operation && (t->text=="*" || t->text=="/") )
	{
		ExpressionPtr result = PriorityOperation::parse(tokens, left); 		
		//we got exit after priority operations will end, parse position will be on + or - or at end
		left = result;	

		t=tokens.peek();
		if (!t || t->type == Token::closing_bracket  ) return left; //just return Value, sorry no operation guys
	}

    //cout << "Operation::parse again" << endl;
	if (t->type == Token::operation && (t->text=="+" || t->text=="-") )
	{
		tokens.get(); //just commit previous peek

		auto_ptr<Operation> result ( new Operation );
		result->_operation = t->text;
		result->_left=left; //smart ptr giveup ownership
        
        //cout << "Operation::parse before token" << endl;
        ExpressionPtr foo=Expression::parse(tokens);
        //cout << "Operation::parse after expression" << endl;
        
        const Token *t=tokens.peek();
        
        if (t != 0 && (t->text=="*" || t->text=="/"))
        {
            //cout << "Operation::parse to priority" << endl;
            foo = PriorityOperation::parse(tokens,foo);
        }
        
        result->_right=foo;
        
        ExpressionPtr bar(result.release());
        return Operation::parse(tokens, bar);
		
	}
	else
	{
		throw logic_error("(Operation) Wtf is that: " + tokens.peek()->text);
	}
}

ExpressionPtr PriorityOperation::parse(Tokenizer& tokens, ExpressionPtr& left)
{
    //cout << "PriorityOperation::parse" << endl;
    
    // left is always assigned for priority operation

	const Token *t=tokens.peek();
	if (!t || t->type == Token::closing_bracket  ) return left; //just return Value, sorry no operation guys

	if (t->type == Token::operation && (t->text=="*" || t->text=="/") )
	{
		tokens.get(); //commit previuos peek

		auto_ptr<PriorityOperation> result ( new PriorityOperation ); 
		result->_operation = t->text;
		result->_left=left;
		result->_right=Expression::parse(tokens);
		ExpressionPtr rs(result.release());

		return PriorityOperation::parse(tokens, rs);

	}
	else if (t->type == Token::operation && (t->text=="+" || t->text=="-") )
	{
		return left;
	}
	else
	{
		throw logic_error("(PriorityOperation) Wtf is that: " + tokens.peek()->text );
	}
}


float Operation::calc()
{
	if (_operation == "+")
	{
		float l=_left.get()?_left->calc():0.0f;
		float r=_right.get()?_right->calc():0.0f;
		return l+r;
	}
	else
	if (_operation == "-")
	{
		float l=_left.get()?_left->calc():0.0f;
		float r=_right.get()?_right->calc():0.0f;
		return l-r;
	}	
	else
	if (_operation == "*")
	{
		float l=_left.get()?_left->calc():1.0f;
		float r=_right.get()?_right->calc():1.0f;
		return  l*r; 
	}	
	else
	if (_operation == "/")
	{
		float l = _left.get()?_left->calc():1.0f;
		float r = _right.get()?_right->calc():1.0f;
		return l/r;
	}
	else
	{
		throw logic_error("Wft: operation udefined");
	}		
}

//returning vector by value, will be slow( O(n*n) actually ), but it is just testing code.
vector<Token> Tokenizer::generate(int level)
{
    //be careful with this value - furmula size in approx 2^level
    if (level > 6)
    {
        Token t;
        char f[100];
        snprintf(f,100,"%d",int(rand() % 100));
        t.text=f;
        t.type=Token::value;
        return vector<Token>(&t,&t+1);
    }

    if (rand() % 10 == 0)
    {
    	vector<Token> result;
    	Token t1,t2;
    	t1.type=Token::opening_bracket;
    	t1.text="(";
    	t2.type=Token::closing_bracket;
    	t2.text=")";
    	result.push_back(t1);
    	vector<Token> r=generate(level+1);
    	result.insert(result.end(),r.begin(),r.end());
    	result.push_back(t2);

    	return result;
    }	

    char op = "+-*/"[rand()%4];
    Token t;
    t.type=Token::operation;
    t.text=op;

    vector<Token> result=generate(level+1);
    result.push_back(t);
    vector<Token> r2=generate(level+1);
    result.insert(result.end(),r2.begin(),r2.end());

    return result;
}

int main()
{	
	try
	{
		//create fake tokenizer
		Tokenizer tk;

		//parse it
		ExpressionPtr null;
		ExpressionPtr foo = Operation::parse(tk,null);
		cout << endl;
		foo->debug("");
		float result = foo->calc();	
		cout << "result = " << result << endl;
	}
	catch(exception& e)
	{
		cout << e.what() << endl;
		return 1;  		
	}

	return 0;
}