

#ifndef __IN_Listeners_H__
#define __IN_Listeners_H__

namespace ui {

class Control;

class OnClickListener
{
public:
    OnClickListener() {}
    ~OnClickListener() {}

    virtual void onClick(Control *widget) = 0;
};

class OnRadioChangedListener
{
public:
    OnRadioChangedListener() {}
    ~OnRadioChangedListener() {}

    virtual void onCheckedChanged(Control *radio_group, int checkedId) = 0;
};

class OnPickersListener
{
public:
    OnPickersListener() {}
    ~OnPickersListener() {}

    virtual void onPickersFocusChanged(Control *picker, int indexFocus) = 0;
};

class OnSliderChangedListener
{
public:
    OnSliderChangedListener() {}
    ~OnSliderChangedListener() {}

    virtual void onSliderChanged(Control *slider, int percentage) = 0;
    virtual void onSliderTriggered(Control *slider, bool triggered) {}
};

class OnListClickListener
{
public:
    OnListClickListener() {}
    ~OnListClickListener() {}

    virtual void onListClicked(Control *list, int index) = 0;
};

class ViewPagerListener
{
public:
    ViewPagerListener() {}
    ~ViewPagerListener() {}

    virtual void onFocusChanged(int indexFocus) = 0;
    virtual void onViewPortChanged(const Point &leftTop) = 0;
};


};

#endif
