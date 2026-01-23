import { Chess, Color, Square, PieceSymbol } from 'chess.js';
import { useState, useEffect, useMemo, useRef, use } from 'react';
import { DndContext, Modifier, PointerSensor, MouseSensor, TouchSensor, useSensor, useSensors } from '@dnd-kit/core';
import { getOwnerDocument } from "@dnd-kit/utilities"
import { snapCenterToCursor } from '@dnd-kit/modifiers';

import DroppableChessCanvasBg, { mapPxToSquare } from './DroppableChessCanvasBg';
import DrawableChessArrowsOverlay, { useMarkings } from './DrawableChessArrowsOverlay';
import { useChessPieceAnimations } from './ChessBoardPieces/AnimatedPieces';
import AnimatedPieces from './ChessBoardPieces/AnimatedPieces';
import DraggablePieces, { useChessDragHandlers } from './ChessBoardPieces/DraggablePieces';
import PromotionView from './PromotionView';

export const chessTypeSquares = [
    "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8",
    "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8",
    "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8",
    "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8",
    "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8",
    "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
    "g1", "g2", "g3", "g4", "g5", "g6", "g7", "g8",
    "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8"
];

export interface Piece {
    piece: PieceSymbol;
    color: Color;
}

interface ChessBoardProps {
    pieces: Record<Square, Piece>;
    pxSize: number;
    colorPerspective: Color;
    showMoveHints?: boolean;
    showCoordinateLabels?: boolean;
    animationSpeedMs?: number;
    userArrowColor?: string;
    userSquareColor?: string;
    allowUserDrawArrows?: boolean;
    allowUserDrawSquares?: boolean;
    drawnSquares?: { square: Square; inProgress: boolean, color: string }[];
    drawnArrows?: { from: Square; to: Square, inProgress: boolean, color: string }[];
    modifyDrawnSquares?: (squares: { square: Square; inProgress: boolean, color: string }[]) => void;
    modifyDrawnArrows?: (arrows: { from: Square; to: Square, inProgress: boolean, color: string }[]) => void;
    dragToMove?: (from: Square, to: Square, promotion?: PieceSymbol) => void;
    getLegalMoves?: (square: Square) => Square[];
    showPreviousMove?: () => { from: Square, to: Square } | null;
    showCheck?: () => Square | null;
}

export const getMovesForSquare = (game: Chess, square: Square): Square[] => {
    const moves = game.moves({ square: square, verbose: true });
    return moves.map(m => m.to);
}

// Chess.js library history method is not performant so the only way to get
// the previous move efficiently is to undo it and redo it
export const getPreviousMove = (game: Chess): { from: Square; to: Square } | null => {
    try {
        const lastMove = game.undo();
        game.move(lastMove!);
        return lastMove;
    } catch {
        return null;
    }
    
}

export const getPieces = (game: Chess) => chessTypeSquares
    .map((square) => {
        const piece = game.get(square as Square);
        if (piece) {
            return {
                square: square as Square,
                piece: piece.type,
                color: piece.color
            };
        }
        return null;
    })
    .filter(x => x !== null)
    .reduce((acc, curr) => {
        acc[curr!.square] = { piece: curr!.piece, color: curr!.color };
        return acc;
    }, {} as Record<Square, Piece>);


export default function ChessBoardView(
    { pieces,
      pxSize, 
      colorPerspective, 
      showMoveHints = true, 
      showCoordinateLabels = true,
      animationSpeedMs = 200,
      userArrowColor = 'rgba(255, 165, 0, 0.65)',
      userSquareColor = 'rgba(255, 0, 0, 0.25)',
      allowUserDrawArrows = true,
      allowUserDrawSquares = true,
      drawnSquares = [],
      drawnArrows = [],
      modifyDrawnSquares = (_) => {},
      modifyDrawnArrows = (_) => {},
      showCheck = () => { return null },
      dragToMove, 
      getLegalMoves, 
      showPreviousMove }: ChessBoardProps
    ) {

    const [suppressAnimations, setSuppressAnimations] = useState(false);
    const [promotionView, setPromotionView] = useState<{ from: Square; to: Square } | null>(null);

    const ignoreMoveAnimation = () => {
        setSuppressAnimations(true);
        setTimeout(() => setSuppressAnimations(false), animationSpeedMs + 50);
    };

    useEffect(() => {
        handleMarkingsMouseLeave();
        clearUserArrows();
        clearUserSquares();
        modifyDrawnArrows([]);
        modifyDrawnSquares([]);
        setPromotionView(null);
    }, [pieces])

    const viewPieces = useMemo(() => {
        if (!promotionView) return pieces;
        ignoreMoveAnimation();
        const { [promotionView.from]: _, ...rest } = pieces;
        return { ...rest, [promotionView.to]: pieces[promotionView.from] };
    }, [pieces, promotionView]) as Record<Square, Piece>;

    const animatingPieces = useChessPieceAnimations({
        pieces: viewPieces,
        pxSize: pxSize,
        colorPerspective: colorPerspective,
        suppressAnimations: suppressAnimations,
        animationSpeedMs: animationSpeedMs
    });

    // Handle drag interactions  
    const { hoveringPieceOver, previewMoves, currentDraggedPieceRef, handleDragMove, handleDragDrop, onDragCancel } = useChessDragHandlers({
        pieces: viewPieces,
        pxSize,
        colorPerspective,
        move: dragToMove ? (...args) => {
            ignoreMoveAnimation();
            dragToMove(...args);
        } : () => {},
        getPromotion: (from: Square, to: Square) => {
            if (!getLegalMoves || getLegalMoves(from).some(sq => sq === to)) {
                setPromotionView({ from, to });
            }
        },
        showAvailableMovesForSquare: !(showMoveHints && dragToMove && getLegalMoves) ? () => [] : (
            getLegalMoves
        )
    });

    const previousMove = (() => {
        if (!showPreviousMove)
            return null;

        if (promotionView)
            return promotionView;

        return showPreviousMove();
    })()

    const { userDrawnSquares, userDrawnArrows, clearUserArrows, clearUserSquares, 
            handleMarkingsMouseDown, handleMarkingsMouseDrag, handleMarkingsMouseUp, handleMarkingsMouseLeave } = useMarkings({
        pxSize,
        colorPerspective,
        arrowColor: userArrowColor,
        squareColor: userSquareColor,
    });

    const handleMouseDown = (event: React.MouseEvent) => {
        if (event.button === 0) {
            handleMarkingsMouseLeave();
            clearUserArrows();
            clearUserSquares();
            return;
        }

        if (event.button !== 2) return;

        if (currentDraggedPieceRef.current) {
            // Cancel drag operation
            const pointerCancelEvent = new PointerEvent('pointercancel', {
                bubbles: true,
                cancelable: true,
                pointerId: 1,
                clientX: event.clientX,
                clientY: event.clientY,
            });
            (event.target as HTMLElement).style.zIndex = '1';
            getOwnerDocument(event.target as Node).dispatchEvent(pointerCancelEvent);
            return;
        }

        handleMarkingsMouseDown(event);
    };

    const sensors = useSensors(
        useSensor(PointerSensor),
        useSensor(TouchSensor),
        // A keyboard sensor would be nice, but dnd-kit's implementation
        // has scrolling effects that don't really work well with a chessboard
        // and there's no real easy fix - 
        // described in https://github.com/clauderic/dnd-kit/issues/1152
    );

    return (
        <div 
            style={{ position: 'relative', width: pxSize, height: pxSize }}
            onMouseDown={handleMouseDown}
            onMouseMove={handleMarkingsMouseDrag}
            onMouseUp={handleMarkingsMouseUp}
            onMouseLeave={handleMarkingsMouseLeave}
            onContextMenu={(e) => e.preventDefault()}
        >
            <DrawableChessArrowsOverlay
                pxSize={pxSize}
                colorPerspective={colorPerspective}
                drawnArrows={[...drawnArrows, ...(allowUserDrawArrows ? userDrawnArrows : [])]}
            />
            <DndContext 
                onDragEnd={handleDragDrop} 
                onDragMove={handleDragMove}
                onDragCancel={onDragCancel}
                modifiers={[snapCenterToCursor, restrictPiecesToBoard]}
                autoScroll={false}
                sensors={sensors}
            >
                <DroppableChessCanvasBg
                    pxSize={pxSize}
                    colorPerspective={colorPerspective}
                    showCoordinateLabels={showCoordinateLabels}
                    highlightHover={hoveringPieceOver}
                    userHighlightColor={userSquareColor}
                    draggingFromSquare={currentDraggedPieceRef.current}
                    squareInCheck={showCheck()}
                    previousMove={previousMove}
                    previewMoves={previewMoves}
                    highlightedSquares={[...drawnSquares, ...(allowUserDrawSquares ? userDrawnSquares : [])]}
                >
                    <DraggablePieces
                        pieces={viewPieces}
                        animatingPieces={animatingPieces}
                        pxSize={pxSize}
                        colorPerspective={colorPerspective}
                    />
                    
                    <AnimatedPieces
                        animatingPieces={animatingPieces}
                    />
                </DroppableChessCanvasBg>
            </DndContext>
            {promotionView && dragToMove && 
            <PromotionView 
                color={colorPerspective} 
                pxSize={pxSize} 
                moveWithPromotion={(piece) => {
                    dragToMove(promotionView.from, promotionView.to, piece);
                    setPromotionView(null);
                }}
                startAtSquare={promotionView.to}
                onClick={() => setPromotionView(null)}
            />}
        </div>
    );
}

// RestrictToParentElement exists, but I wasn't a fan of the implmentation
// as it restricted the entire width of the piece to be within the parent.
// This implementation has the piece center able to go to the edge of the board
const restrictPiecesToBoard: Modifier = ({
    containerNodeRect,
    draggingNodeRect,
    transform,
}) => {

    if (!draggingNodeRect || !containerNodeRect) {
        return transform;
    }

    const squareSize = containerNodeRect.width / 8;
 
    const originalPos = {
        top:   draggingNodeRect.top - containerNodeRect.top,
        left:  draggingNodeRect.left - containerNodeRect.left,
    }

    const relativePos = {
        top:   originalPos.top + transform.y,
        left:  originalPos.left + transform.x,
    }
    
    // Left guard has an extra pixel so that when a piece is on the edge, 
    // the player can't make an unintentional move because the center of
    // the piece won't be above a square anymore
    const leftGuard = -Math.ceil((squareSize / 2) + 1)
    const rightGuard = Math.ceil(containerNodeRect.width - squareSize - leftGuard)

    if (relativePos.left <= leftGuard) {
        transform.x = leftGuard - originalPos.left
    }

    if (relativePos.left >= rightGuard) {
        transform.x = rightGuard - originalPos.left
    }

    if (relativePos.top <= leftGuard) {
        transform.y = leftGuard - originalPos.top
    }
    
    if (relativePos.top >= rightGuard) {
        transform.y = rightGuard - originalPos.top
    }

    return transform;
};
