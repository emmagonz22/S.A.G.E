import React from 'react';
import { Carousel } from 'antd';

interface GalleryProps {
  images?: Array<{
    src: string;
    alt?: string;
  }>;
  height?: string | number;
  width?: string | number;
  maxImages?: number;
}

const Gallery: React.FC<GalleryProps> = ({ 
  images = [], 
  height = '400px',
  width = '100%',
  maxImages = 4
}) => {
  // Default placeholder images if none are provided
  const defaultImages = [
    { src: '/placeholder1.jpg', alt: 'Placeholder 1' },
    { src: '/placeholder2.jpg', alt: 'Placeholder 2' },
    { src: '/placeholder3.jpg', alt: 'Placeholder 3' },
    { src: '/placeholder4.jpg', alt: 'Placeholder 4' },
  ];

  // Use provided images or fallback to defaults, limited to maxImages
  const displayImages = images.length > 0 
    ? images.slice(0, maxImages) 
    : defaultImages.slice(0, maxImages);
    
  // Calculate responsive dimensions based on image count
  const calculatedWidth = typeof width === 'string' && width.includes('%') 
    ? `${parseInt(width) * (displayImages.length / 4)}%`
    : typeof width === 'number'
      ? (width * (displayImages.length / 4))
      : `${(displayImages.length / 4) * 100}%`;
      
  // Use original width if there are 4 images or the calculated width is wider than original
  const responsiveWidth = displayImages.length >= 4 ? width : calculatedWidth;

  const carouselStyle: React.CSSProperties = {
    width: responsiveWidth,
    margin: '0 auto',
  };
  
  const imageStyle: React.CSSProperties = {
    height,
    width: '100%',
    objectFit: 'cover',
    borderRadius: '8px',
  };

  return (
    <div style={carouselStyle}>
      <Carousel autoplay>
        {displayImages.map((image, index) => (
          <div key={index} className="carousel-item">
            <img 
              src={image.src} 
              alt={image.alt || `Image ${index + 1}`} 
              style={imageStyle}
              onError={(e) => {
                // Fallback for failed image loads
                e.currentTarget.src = `data:image/svg+xml;charset=UTF-8,%3Csvg width='800' height='${height}' xmlns='http://www.w3.org/2000/svg'%3E%3Crect width='800' height='${height}' fill='%23CCCCCC'/%3E%3Ctext x='400' y='${Number(height.toString().replace('px', '')) / 2}' dominant-baseline='middle' text-anchor='middle' font-family='Arial' font-size='24' fill='%23666666'%3EImage ${index + 1}%3C/text%3E%3C/svg%3E`;
              }}
            />
          </div>
        ))}
      </Carousel>
    </div>
  );
};

export default Gallery;
