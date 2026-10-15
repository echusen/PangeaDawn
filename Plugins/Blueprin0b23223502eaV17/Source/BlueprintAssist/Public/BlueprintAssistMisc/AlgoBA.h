// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define BA_LAMBDA_CAP(Capture, FuncName,...) [Capture](auto&&... Args){ return FuncName(Forward<decltype(Args)>(Args)..., ##__VA_ARGS__); }
#define BA_LAMBDA(FuncName,...) BA_LAMBDA_CAP(&, FuncName, ##__VA_ARGS__)

// I don't understand how this works?!
template <typename T = void>
struct TPlusEq
{
	FORCEINLINE void operator()(T& A, const T& B) { A += B; }
};

template <>
struct TPlusEq<void>
{
	template <typename U, typename V>
	FORCEINLINE void operator()(U&& A, V&& B) { A += B; }
};

template <typename T = void>
struct TAppend
{
	FORCEINLINE void operator()(T& A, const T& B) { A.Append(B); }
};

template <>
struct TAppend<void>
{
	template <typename U, typename V>
	FORCEINLINE void operator()(U&& A, V&& B) { A.Append(B); }
};

template <typename T = void>
struct TAdd
{
	FORCEINLINE void operator()(T& A, const T& B) { A.Add(B); }
};

template <>
struct TAdd<void>
{
	template <typename U, typename V>
	FORCEINLINE void operator()(U&& A, V&& B) { A.Add(B); }
};

namespace AlgoBA
{
	template <typename InT, typename OutT, typename AccT>
	FORCEINLINE void Accumulate(const InT& Input, OutT& Output, AccT Acc)
	{
		for (const auto& InputElem : Input)
		{
			Invoke(Acc, Output, InputElem);
		}
	}

	template <typename InT, typename OutT>
	FORCEINLINE void Accumulate(const InT& Input, OutT& Output)
	{
		Accumulate(Input, Output, TPlusEq());
	}

	template <typename InT, typename OutT, typename TransformT, typename AccT>
	FORCEINLINE void TransformAccumulate(const InT& Input, OutT& Output, TransformT Trans, AccT Acc)
	{
		for (const auto& InputElem : Input)
		{
			Invoke(Acc, Output, Invoke(Trans, InputElem));
		}
	}

	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void TransformAccumulate(const InT& Input, OutT& Output, TransformT Trans)
	{
		TransformAccumulate(Input, Output, Trans, TPlusEq());
	}
}
