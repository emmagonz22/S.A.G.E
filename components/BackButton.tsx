import React from 'react';
import { StyleSheet } from 'react-native';
import { ChevronLeft } from '@tamagui/lucide-icons';
import { useNavigation } from '@react-navigation/native';
import { Button, Text } from 'tamagui';

const BackButton = () => {
  const navigation = useNavigation();
 
  return (
    <Button style={styles.container} onPress={() => navigation.goBack()}>
        <ChevronLeft/>
        <Text style={styles.text}>Back</Text>
    </Button>
  );
};

const styles = StyleSheet.create({
  container: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  text: {
    color: 'black',
    marginLeft: 2,
  },
  chevron: {
    color: 'red', 
    width: 24,
    height: 24,
  }
});

export default BackButton;