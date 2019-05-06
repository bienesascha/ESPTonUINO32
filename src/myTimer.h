#ifndef myTimer_h
#define myTimer_h

// the #include statment and code go here...

class myTimer
{
public:
    myTimer(void);
    void set_max_delay(unsigned long v);
    void set(void);
    boolean check(void);

private:
    unsigned long max_delay;
    unsigned long last_set;
};

myTimer::myTimer(void)
{
    max_delay = 3600000UL; // default 1 hour
}

void myTimer::set_max_delay(unsigned long v)
{
    max_delay = v;
    //Serial.print("set_max_delay:");Serial.println(v);
    set();
}

void myTimer::set()
{
    last_set = millis();
}

boolean myTimer::check()
{
    unsigned long now = millis();
    if (now - last_set > max_delay) {
        last_set = now;
        return true;
    }
    return false;
}

#endif
