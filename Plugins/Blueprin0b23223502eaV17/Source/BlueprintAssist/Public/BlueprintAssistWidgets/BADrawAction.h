// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"

class FBAGraphHandler;
class SGraphPanel;

struct FBADrawContext
{
	const FGeometry& AllottedGeometry;
	FSlateWindowElementList& OutDrawElements;
	int32 LayerId;
	TSharedPtr<SGraphPanel> GraphPanel;
	TSharedPtr<FBAGraphHandler> GraphHandler;

	void DrawWidgetAsBox(TSharedPtr<SWidget> Widget, const FSlateRect& WidgetBounds, const FLinearColor& Color) const;
	void DrawTextOverWidget(const FText& Text, const FSlateFontInfo& FontInfo, TSharedPtr<SWidget> Widget, const FSlateRect& WidgetBounds, const FLinearColor& Color) const;
	void DrawArrow(FVector2D Start, FVector2D End, FLinearColor Color, int LineWidth = 5.0f, float ArrowSize = 30.0f, float ArrowAngle = 30.0f) const;
};

class IBADrawAction
{
public:
	virtual ~IBADrawAction() = default;
	virtual void Draw(const FBADrawContext& Ctx) const = 0;
	virtual bool Update(float DeltaTime) { return true; } // false to destroy action
};

class FBADrawLineAction : public IBADrawAction
{
public:
	TArray<FVector2D> Points;
	FLinearColor Color;
	float Thickness;

	FBADrawLineAction(const TArray<FVector2D>& InPoints, const FLinearColor& InColor = FLinearColor::Green, float InThickness = 2.0f)
		: Points(InPoints), Color(InColor), Thickness(InThickness)
	{
	}

	virtual void Draw(const FBADrawContext& Ctx) const override;
};

class FBADrawBoundsAction : public IBADrawAction
{
public:
	FSlateRect Bounds;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 2.0f;

	FBADrawBoundsAction(const FSlateRect& InBounds, const FLinearColor& InColor = FLinearColor::White)
		: Bounds(InBounds), Color(InColor)
	{
	}

	virtual void Draw(const FBADrawContext& Ctx) const override;
};

class FBADrawNodeAction : public IBADrawAction
{
public:
	TWeakObjectPtr<UEdGraphNode> Node;
	TSharedPtr<SGraphPanel> GraphPanel;
	FLinearColor Color = FLinearColor::White;

	FBADrawNodeAction(TWeakObjectPtr<UEdGraphNode> InNode, TSharedPtr<SGraphPanel> InGraphPanel, const FLinearColor& InColor = FLinearColor::White)
		: Node(InNode), GraphPanel(InGraphPanel), Color(InColor)
	{
	}

	virtual void Draw(const FBADrawContext& Ctx) const override;
};

class FBADrawTextOnWidgetAction : public IBADrawAction
{
public:
	FText Text;
	TWeakPtr<SWidget> Widget;
	FSlateRect WidgetBounds;
	FLinearColor Color = FLinearColor::White;

	FBADrawTextOnWidgetAction(const FText& InText, TWeakPtr<SWidget> InWidget, const FSlateRect& InWidgetBounds, const FLinearColor& InColor = FLinearColor::White)
		: Text(InText), Widget(InWidget), WidgetBounds(InWidgetBounds), Color(InColor)
	{
	}

	virtual void Draw(const FBADrawContext& Ctx) const override;
};

class FBADrawPinLinkAction : public IBADrawAction
{
public:
	FBAGraphPinHandle HandleA;
	FBAGraphPinHandle HandleB;
	FLinearColor Color = FLinearColor::Green;

	FBADrawPinLinkAction(const FBAGraphPinHandle& PinHandleA, const FBAGraphPinHandle& PinHandleB, const FLinearColor& InColor = FLinearColor::Green)
		: HandleA(PinHandleA), HandleB(PinHandleB), Color(InColor)
	{
	}

	virtual void Draw(const FBADrawContext& Ctx) const override;
};

class FBADrawTimedAction : public IBADrawAction
{
public:
	TSharedPtr<IBADrawAction> Action;
	float LifeTime;

	FBADrawTimedAction(TSharedPtr<IBADrawAction> InAction, float InLifetime) : Action(InAction), LifeTime(InLifetime)
	{
	}

	virtual bool Update(float DeltaTime) override
	{
		LifeTime -= DeltaTime;
		return LifeTime > 0;
	}

	virtual void Draw(const FBADrawContext& Ctx) const override
	{
		Action->Draw(Ctx);
	}
};

class FBADrawQueueAction : public IBADrawAction
{
	TArray<TSharedPtr<IBADrawAction>> Queue;
	int ActiveIndex = 0;
	float Interval;
	float Timer;
	bool bLoop = false;

public:
	FBADrawQueueAction(float InInterval = 2.0f)
		: Interval(InInterval)
		, Timer(Interval)
	{
	}

	FBADrawQueueAction(const TArray<TSharedPtr<IBADrawAction>>& InActions, float InInterval = 2.0f)
		: Queue(InActions)
		, Interval(InInterval)
		, Timer(Interval)
	{
	}

	void Add(TSharedPtr<IBADrawAction> Action)
	{
		Queue.Add(Action);
	}


	virtual bool Update(float DeltaTime) override
	{
		Timer -= DeltaTime;
		if (Timer <= 0)
		{
			Timer = Interval;

			if (bLoop)
			{
				ActiveIndex = Queue.Num() % (ActiveIndex + 1);
			}
			else
			{
				ActiveIndex += 1;
			}
		}

		if (!bLoop)
		{
			return Queue.IsValidIndex(ActiveIndex);
		}

		return true;
	}

	virtual void Draw(const FBADrawContext& Ctx) const override
	{
		TSharedPtr<IBADrawAction> CurrentActive = Queue.IsValidIndex(ActiveIndex) ? Queue[ActiveIndex] : nullptr;
		if (CurrentActive)
		{
			CurrentActive->Draw(Ctx);
		}
	}
};

class FBADrawListAction : public IBADrawAction
{
	TArray<TSharedPtr<IBADrawAction>> Actions;

public:
	FBADrawListAction() = default;
	FBADrawListAction(const TArray<TSharedPtr<IBADrawAction>>& InActions) : Actions(InActions) {}

	void Add(TSharedPtr<IBADrawAction> Action)
	{
		Actions.Add(Action);
	}

	void Add(const TArray<TSharedPtr<IBADrawAction>>& InActions)
	{
		Actions.Append(InActions);
	}

	virtual void Draw(const FBADrawContext& Ctx) const override
	{
		for (const auto& Action : Actions)
		{
			Action->Draw(Ctx);
		}
	}
};

class FBADrawBuilder
{
public:
	FBADrawBuilder() = default;

	FBADrawBuilder& Add(TSharedPtr<IBADrawAction> InAction)
	{
		PendingActions.Add(InAction);
		return *this;
	}

	template <typename T, typename... ArgTypes>
	FBADrawBuilder& Add(ArgTypes&& ...Args)
	{
		TSharedPtr<T> Action = MakeShared<T>(Forward<ArgTypes>(Args)...);
		PendingActions.Add(Action);
		return *this;
	}

	FBADrawBuilder& Timed(float InDuration)
	{
		if (TSharedPtr<IBADrawAction> Combined = CombinePendingActions())
		{
			TSharedRef<FBADrawTimedAction> TimedAction = MakeShared<FBADrawTimedAction>(Combined, InDuration);
			ConsumeActions(TimedAction);
		}

		return *this;
	}

	FBADrawBuilder& Queue(float InInterval = 2.0f)
	{
		if (PendingActions.Num())
		{
			TSharedRef<FBADrawQueueAction> QueueAction = MakeShared<FBADrawQueueAction>(PendingActions, InInterval);
			ConsumeActions(QueueAction);
		}

		return *this;
	}

	FBADrawBuilder& List()
	{
		auto ListAction = MakeShared<FBADrawListAction>(PendingActions);
		ConsumeActions(ListAction);
		return *this;
	}

	TSharedPtr<IBADrawAction> Get()
	{
		return LastBuiltAction;
	}

private:
	void ConsumeActions(TSharedPtr<IBADrawAction> Action)
	{
		PendingActions.Empty();
		LastBuiltAction = Action;
	}

	TSharedPtr<IBADrawAction> CombinePendingActions()
	{
		if (PendingActions.Num() > 1)
		{
			return MakeShared<FBADrawListAction>(PendingActions);
		}

		if (PendingActions.Num() == 1)
		{
			return PendingActions[0];
		}

		return nullptr;
	}

	TArray<TSharedPtr<IBADrawAction>> PendingActions;

	TSharedPtr<IBADrawAction> LastBuiltAction;

	FLinearColor QueueColor = FLinearColor::White;
};
